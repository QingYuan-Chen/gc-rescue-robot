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
    __HAL_TIM_SET_COMPARE(timer, TIM_CHANNEL_1, 0U);
    __HAL_TIM_SET_COMPARE(timer, side == 0U ? TIM_CHANNEL_2 : TIM_CHANNEL_4, 0U);
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

void motor_init(uint32_t now_ms) { motor_stop(now_ms); }

static void set_compare(unsigned side, int16_t duty)
{
    const uint32_t magnitude = (uint32_t)(duty < 0 ? -duty : duty);
    const uint32_t in1 = duty > 0 ? WHEEL_PWM_PERIOD : WHEEL_PWM_PERIOD - magnitude;
    const uint32_t in2 = duty > 0 ? WHEEL_PWM_PERIOD - magnitude : WHEEL_PWM_PERIOD;
    TIM_HandleTypeDef *timer = side == 0U ? &htim2 : &htim1;
    __HAL_TIM_SET_COMPARE(timer, TIM_CHANNEL_1, in1);
    __HAL_TIM_SET_COMPARE(timer, side == 0U ? TIM_CHANNEL_2 : TIM_CHANNEL_4, in2);
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

static bool update_side(unsigned side, int16_t duty, uint32_t now_ms)
{
    motor_state_t *state = &motors[side];
    if (duty == 0)
    {
        if (state->phase != MOTOR_OFF)
        {
            stop_side(side, now_ms);
        }
        return true;
    }
    if (state->phase == MOTOR_RUNNING && ((duty > 0) != (state->applied > 0)))
    {
        /* 换向先完整经过待机和唤醒时序；不代表机械上已经停稳。 */
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

bool motor_update(int16_t left, int16_t right, uint32_t now_ms)
{
    /* 底层只约束物理占空比；首次联调的更小范围由 wheel_io 入口限制。 */
    if (left < -1000 || left > 1000 || right < -1000 || right > 1000 ||
        !update_side(0U, (int16_t)-left, now_ms) || !update_side(1U, right, now_ms))
    {
        motor_stop(now_ms);
        return false;
    }
    return true;
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
