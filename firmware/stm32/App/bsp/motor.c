#include "motor.h"

#include "config/wheel_config.h"
#include "tim.h"

typedef struct
{
    motor_phase_t phase;
    uint32_t since_ms;
    int16_t applied;
} motor_state_t;

/* 车轮周期任务独占电机输出，左 M2，右 M1。 */
static motor_state_t motors[2];
static volatile bool guard_armed;
static volatile bool guard_tripped = true;
static volatile uint32_t guard_completed_ms;

void motor_force_outputs_low(void)
{
    /* 先将复用输出同步切低，再接管 GPIO，避免逐脚切换产生反向组合。 */
    if (__HAL_RCC_TIM1_IS_CLK_ENABLED())
    {
        TIM1->CR1 &= ~TIM_CR1_UDIS;
        TIM1->CCR1 = 0U;
        TIM1->CCR4 = 0U;
        TIM1->EGR = TIM_EGR_UG;
    }
    if (__HAL_RCC_TIM2_IS_CLK_ENABLED())
    {
        TIM2->CR1 &= ~TIM_CR1_UDIS;
        TIM2->CCR1 = 0U;
        TIM2->CCR2 = 0U;
        TIM2->EGR = TIM_EGR_UG;
    }
    GPIOA->BSRR = (uint32_t)(M1_IN1_Pin | M1_IN2_Pin | M2_IN1_Pin) << 16U;
    GPIOB->BSRR = (uint32_t)M2_IN2_Pin << 16U;
    const uint32_t a_mask = (3UL << (8U * 2U)) | (3UL << (11U * 2U)) | (3UL << (5U * 2U));
    const uint32_t a_out = (1UL << (8U * 2U)) | (1UL << (11U * 2U)) | (1UL << (5U * 2U));
    GPIOA->MODER = (GPIOA->MODER & ~a_mask) | a_out;
    GPIOB->MODER = (GPIOB->MODER & ~(3UL << (3U * 2U))) | (1UL << (3U * 2U));
}

void motor_guard_tick_isr(uint32_t now_ms)
{
    if (guard_armed && now_ms - guard_completed_ms >= 30U)
    {
        guard_tripped = true;
        guard_armed = false;
        motor_force_outputs_low();
    }
}

bool motor_guard_fault(void) { return guard_tripped; }

void motor_guard_begin(uint32_t now_ms)
{
    if (!guard_armed) { guard_completed_ms = now_ms; }
    guard_armed = true;
}

void motor_guard_complete(uint32_t now_ms)
{
    guard_completed_ms = now_ms;
    guard_armed = motors[0].phase != MOTOR_OFF || motors[1].phase != MOTOR_OFF;
}

static void pin_config(GPIO_TypeDef *port, uint16_t pins, uint32_t mode, GPIO_PinState level)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pins;
    gpio.Mode = mode;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_WritePin(port, pins, level);
    HAL_GPIO_Init(port, &gpio);
}

static void configure_pins(unsigned side, uint32_t mode, GPIO_PinState level)
{
    if (side == 0U)
    {
        pin_config(GPIOA, M2_IN1_Pin, mode, level);
        pin_config(GPIOB, M2_IN2_Pin, mode, level);
    }
    else
    {
        pin_config(GPIOA, M1_IN1_Pin | M1_IN2_Pin, mode, level);
    }
}

static void stop_side(unsigned side, uint32_t now_ms)
{
    TIM_HandleTypeDef *timer = side == 0U ? &htim2 : &htim1;
    /* 同一个定时器同时提交两路低电平，再把引脚交还 GPIO。 */
    timer->Instance->CR1 |= TIM_CR1_UDIS;
    __HAL_TIM_SET_COMPARE(timer, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(timer, side == 0U ? TIM_CHANNEL_2 : TIM_CHANNEL_4, 0U);
    timer->Instance->CR1 &= ~TIM_CR1_UDIS;
    timer->Instance->EGR = TIM_EGR_UG;
    configure_pins(side, GPIO_MODE_OUTPUT_PP, GPIO_PIN_RESET);
    HAL_TIM_PWM_Stop(timer, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(timer, side == 0U ? TIM_CHANNEL_2 : TIM_CHANNEL_4);
    motors[side] = (motor_state_t){.phase = MOTOR_OFF, .since_ms = now_ms};
}

void motor_stop(uint32_t now_ms)
{
    stop_side(0U, now_ms);
    stop_side(1U, now_ms);
}

void motor_init(uint32_t now_ms)
{
    guard_armed = false;
    guard_tripped = false;
    motor_stop(now_ms);
}

static void set_compare(unsigned side, int16_t duty)
{
    const uint32_t magnitude = (uint32_t)(duty < 0 ? -duty : duty);
    const uint32_t in1 = duty > 0 ? WHEEL_PWM_PERIOD : WHEEL_PWM_PERIOD - magnitude;
    const uint32_t in2 = duty > 0 ? WHEEL_PWM_PERIOD - magnitude : WHEEL_PWM_PERIOD;
    TIM_HandleTypeDef *timer = side == 0U ? &htim2 : &htim1;
    /* 关闭更新事件，保证两路预装载同一周期生效。 */
    timer->Instance->CR1 |= TIM_CR1_UDIS;
    __HAL_TIM_SET_COMPARE(timer, TIM_CHANNEL_1, in1);
    __HAL_TIM_SET_COMPARE(timer, side == 0U ? TIM_CHANNEL_2 : TIM_CHANNEL_4, in2);
    timer->Instance->CR1 &= ~TIM_CR1_UDIS;
}

static bool start_side(unsigned side)
{
    TIM_HandleTypeDef *timer = side == 0U ? &htim2 : &htim1;
    timer->Instance->EGR = TIM_EGR_UG;
    HAL_StatusTypeDef first = HAL_TIM_PWM_Start(timer, TIM_CHANNEL_1);
    HAL_StatusTypeDef second = HAL_TIM_PWM_Start(timer, side == 0U ? TIM_CHANNEL_2 : TIM_CHANNEL_4);
    if (first != HAL_OK || second != HAL_OK) { return false; }
    configure_pins(side, GPIO_MODE_AF_PP, GPIO_PIN_SET);
    return true;
}

static bool update_side(unsigned side, int16_t duty, bool brake, bool continuous, uint32_t now_ms)
{
    motor_state_t *state = &motors[side];
    if (duty == 0 && !brake)
    {
        if (state->phase != MOTOR_OFF)
        {
            stop_side(side, now_ms);
        }
        return true;
    }
    if (!continuous && state->phase == MOTOR_RUNNING &&
        ((duty > 0) != (state->applied > 0)))
    {
        stop_side(side, now_ms);
        return true;
    }
    /* 每次调用最多推进一个阶段；等待时间包含在请求期限内，不忙等。 */
    if (state->phase == MOTOR_OFF)
    {
        if (now_ms - state->since_ms >= WHEEL_STANDBY_MIN_MS)
        {
            /* 双高仅短路制动，先完成退出待机，再接入 PWM。 */
            configure_pins(side, GPIO_MODE_OUTPUT_PP, GPIO_PIN_SET);
            state->phase = MOTOR_WAKING;
            state->since_ms = now_ms;
        }
        return true;
    }
    if (state->phase == MOTOR_WAKING && now_ms - state->since_ms < WHEEL_WAKE_MIN_MS)
    {
        return true;
    }
    set_compare(side, duty);
    if (state->phase == MOTOR_WAKING && !start_side(side))
    {
        return false;
    }
    state->phase = MOTOR_RUNNING;
    state->applied = duty;
    return true;
}

static bool update_both(int16_t left, int16_t right, bool brake_left, bool brake_right,
                        bool continuous, uint32_t now_ms)
{
    /* 底层只约束物理占空比；首次联调的更小范围由 wheel_io 入口限制。 */
    if (left < -1000 || left > 1000 || right < -1000 || right > 1000 ||
        guard_tripped || !update_side(0U, (int16_t)-left, brake_left, continuous, now_ms) ||
        !update_side(1U, right, brake_right, continuous, now_ms))
    {
        motor_stop(now_ms);
        return false;
    }
    return true;
}

bool motor_update(int16_t left, int16_t right, uint32_t now_ms)
{
    return update_both(left, right, false, false, false, now_ms);
}

bool motor_update_with_brake(int16_t left, int16_t right, bool brake_left, bool brake_right,
                             uint32_t now_ms)
{
    return update_both(left, right, brake_left, brake_right, true, now_ms);
}

void motor_get_status(motor_status_t *status)
{
    *status = (motor_status_t){
        .applied_left = (int16_t)-motors[0].applied,
        .applied_right = motors[1].applied,
        .phase_left = motors[0].phase,
        .phase_right = motors[1].phase,
    };
}
