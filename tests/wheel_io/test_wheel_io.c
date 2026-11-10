#include "tim.h"
#include "bsp/encoder.h"
#include "bsp/motor.h"
#include "runtime/wheel_io.h"
#include "debug/motor_commands.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GPIO_TypeDef test_gpio_a, test_gpio_b, test_gpio_c, test_gpio_d, test_gpio_e;
TIM_TypeDef test_tim1, test_tim2, test_tim5, test_tim3, test_tim4, test_tim6, test_tim8, test_tim16, test_tim17;
uint32_t test_clocks, uwTickPrio;
extern TIM_HandleTypeDef htim6;
static uint32_t now, divider, pclk = 64000000U;
static unsigned critical_depth, starts, fail_start, active_pwm, active_encoder;
static unsigned pwm_configs, encoder_configs, base_starts;

void Error_Handler(void)
{
    abort();
}
void test_enter_critical(void)
{
    critical_depth++;
}
void test_exit_critical(void)
{
    assert(critical_depth > 0);
    critical_depth--;
}
uint32_t osKernelGetTickCount(void)
{
    return now;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pins, GPIO_PinState level)
{
    if (level == GPIO_PIN_SET)
    {
        port->ODR |= pins;
    }
    else
    {
        port->ODR &= ~pins;
    }
}

void HAL_GPIO_Init(GPIO_TypeDef *port, const GPIO_InitTypeDef *config)
{
    if (config->Mode == GPIO_MODE_AF_PP)
    {
        if (config->Pull == GPIO_PULLUP)
        {
            assert(port == GPIOA && config->Alternate == 2U);
            assert(config->Pin == 0xC0U || config->Pin == 3U);
        }
        else
        {
            assert(config->Alternate == 1U);
            assert((port->ODR & config->Pin) == config->Pin);
            if (port == GPIOB) { assert(config->Pin == 8U && (active_pwm & 3U) == 3U); }
            else if (config->Pin == 0x20U) { assert((active_pwm & 3U) == 3U); }
            else { assert(port == GPIOA && config->Pin == 0x900U && (active_pwm & 12U) == 12U); }
        }
    }
    for (unsigned pin = 0; pin < 16; pin++)
    {
        if (config->Pin & (1U << pin))
        {
            port->MODER = (port->MODER & ~(3U << (pin * 2))) | config->Mode << (pin * 2);
        }
    }
}

HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *timer)
{
    assert(timer->Init.Prescaler == 11U && timer->Init.Period == 999U);
    HAL_TIM_PWM_MspInit(timer);
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *timer, const TIM_OC_InitTypeDef *oc,
                                            uint32_t channel)
{
    assert(oc->OCMode == TIM_OCMODE_PWM1 && oc->Pulse == 0U);
    assert(oc->OCPolarity == TIM_OCPOLARITY_HIGH && oc->OCNPolarity == TIM_OCNPOLARITY_HIGH);
    assert(channel == TIM_CHANNEL_1 || (timer->Instance == TIM2 && channel == TIM_CHANNEL_2) || (timer->Instance == TIM1 && channel == TIM_CHANNEL_4));
    pwm_configs++;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_Encoder_Init(TIM_HandleTypeDef *timer, const TIM_Encoder_InitTypeDef *ic)
{
    assert(timer->Init.Period == 65535U && timer->Init.Prescaler == 0U);
    assert(ic->EncoderMode == TIM_ENCODERMODE_TI12);
    assert(ic->IC1Polarity == TIM_ICPOLARITY_RISING && ic->IC2Polarity == TIM_ICPOLARITY_RISING);
    assert(ic->IC1Filter == 10U && ic->IC2Filter == 10U);
    assert(ic->IC1Selection == TIM_ICSELECTION_DIRECTTI &&
           ic->IC2Selection == TIM_ICSELECTION_DIRECTTI);
    HAL_TIM_Encoder_MspInit(timer);
    encoder_configs++;
    return HAL_OK;
}

static unsigned pwm_bit(TIM_HandleTypeDef *timer, uint32_t channel, bool complementary)
{
    assert(!complementary);
    if (timer->Instance == TIM2) { assert(channel == TIM_CHANNEL_1 || channel == TIM_CHANNEL_2); return channel == TIM_CHANNEL_1 ? 1U : 2U; }
    assert(timer->Instance == TIM1 && (channel == TIM_CHANNEL_1 || channel == TIM_CHANNEL_4));
    return channel == TIM_CHANNEL_1 ? 4U : 8U;
}
static HAL_StatusTypeDef start_pwm(TIM_HandleTypeDef *timer, uint32_t channel, bool complementary)
{
    unsigned bit = pwm_bit(timer, channel, complementary);
    assert(timer->Instance->EGR == TIM_EGR_UG);
    if (timer->Instance == TIM2) { assert((GPIOA->ODR & 0x20U) && (GPIOB->ODR & 8U)); }
    else { assert((GPIOA->ODR & 0x900U) == 0x900U); }
    starts++;
    if (starts == fail_start) { return HAL_ERROR; }
    active_pwm |= bit;
    return HAL_OK;
}
static HAL_StatusTypeDef stop_pwm(TIM_HandleTypeDef *timer, uint32_t channel, bool complementary)
{
    unsigned bit = pwm_bit(timer, channel, complementary);
    if (timer->Instance == TIM2) { assert(!(GPIOA->ODR & 0x20U) && !(GPIOB->ODR & 8U)); }
    else { assert(!(GPIOA->ODR & 0x900U)); }
    active_pwm &= ~bit;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *t, uint32_t c)
{
    return start_pwm(t, c, false);
}
HAL_StatusTypeDef HAL_TIMEx_PWMN_Start(TIM_HandleTypeDef *t, uint32_t c)
{
    return start_pwm(t, c, true);
}
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *t, uint32_t c)
{
    return stop_pwm(t, c, false);
}
HAL_StatusTypeDef HAL_TIMEx_PWMN_Stop(TIM_HandleTypeDef *t, uint32_t c)
{
    return stop_pwm(t, c, true);
}
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *timer, uint32_t channel)
{
    assert(channel == TIM_CHANNEL_ALL);
    assert(timer->Instance == TIM3 || timer->Instance == TIM5);
    starts++;
    if (starts == fail_start)
    {
        return HAL_ERROR;
    }
    active_encoder |= timer->Instance == TIM3 ? 1U : 2U;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Encoder_Stop(TIM_HandleTypeDef *timer, uint32_t channel)
{
    assert(channel == TIM_CHANNEL_ALL);
    active_encoder &= ~(timer->Instance == TIM3 ? 1U : 2U);
    return HAL_OK;
}
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *clocks, uint32_t *latency)
{
    clocks->APB1CLKDivider = divider;
    *latency = 0;
}
uint32_t HAL_RCC_GetPCLK1Freq(void)
{
    return pclk;
}
void HAL_NVIC_SetPriority(int irq, uint32_t preempt, uint32_t sub)
{
    assert(irq == TIM6_DAC_IRQn && preempt == 15U && sub == 0U);
}
void HAL_NVIC_ClearPendingIRQ(int irq)
{
    assert(irq == TIM6_DAC_IRQn);
}
void HAL_NVIC_EnableIRQ(int irq)
{
    assert(irq == TIM6_DAC_IRQn);
}
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *timer)
{
    assert(timer->Instance == TIM6 && timer->Init.Period == 999U);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *timer)
{
    assert(timer->Instance == TIM6);
    timer->Instance->DIER |= TIM_IT_UPDATE;
    base_starts++;
    return HAL_OK;
}

static wheel_io_status_t status(void)
{
    wheel_io_status_t value;
    wheel_io_get_status(&value);
    assert(critical_depth == 0U);
    return value;
}
static void step(uint32_t dt)
{
    now += dt;
    wheel_io_step(now, false);
}
static void assert_off(void)
{
    motor_status_t value;
    motor_get_status(&value);
    assert(value.phase_left == MOTOR_OFF && value.phase_right == MOTOR_OFF);
    assert(value.applied_left == 0 && value.applied_right == 0);
    assert(active_pwm == 0U && (GPIOA->ODR & 0x920U) == 0U && (GPIOB->ODR & 8U) == 0U);
}
static void init(void)
{
    MX_TIM1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_TIM5_Init();
    assert(pwm_configs == 4U && encoder_configs == 2U && starts == 0U);
    assert(wheel_io_init(now));
    assert(active_encoder == 3U);
    assert_off();
}
static void check_startup(void)
{
    for (unsigned i = 0; i < 100; i++)
    {
        step(10);
        assert_off();
    }
    assert(status().ready && !status().fault && status().remaining_ms == 0U);
}
static void check_configuration(void)
{
    assert(HAL_InitTick(15) == HAL_OK && htim6.Init.Prescaler == 63U);
    divider = 4U;
    pclk = 120000000U;
    assert(HAL_InitTick(15) == HAL_OK && htim6.Init.Prescaler == 239U);
    assert(base_starts == 2U && uwTickPrio == 15U);
    HAL_SuspendTick();
    assert(TIM6->DIER == 0);
    HAL_ResumeTick();
    assert(TIM6->DIER == TIM_IT_UPDATE);
    assert(HAL_InitTick(16) == HAL_ERROR);
    for (unsigned i = 0; i < 500; i++)
    {
        step(10);
    }
    assert_off();
    assert(!status().fault);
}
static void check_encoder(void)
{
    encoder_sample_t value;
    TIM5->CNT = 65534;
    TIM3->CNT = 7;
    encoder_sample(&value);
    assert(value.delta_left == 2 && value.delta_right == 7);
    TIM5->CNT = 3;
    TIM3->CNT = 65533;
    encoder_sample(&value);
    assert(value.delta_left == -5 && value.delta_right == -10);
    assert(value.total_left == -3 && value.total_right == -3);
    TIM5->CNT = 65535;
    TIM3->CNT = 0;
    encoder_sample(&value);
    assert(value.delta_left == 4 && value.delta_right == 3);
    assert(value.total_left == 1 && value.total_right == 0);
    for (unsigned i = 0; i < 100000; i++)
    {
        TIM5->CNT = (TIM5->CNT + 30000U) & 65535U;
        encoder_sample(&value);
    }
    assert(value.total_left == -INT64_C(2999999999));
    active_encoder = 0;
    fail_start = starts + 2;
    assert(!encoder_init() && active_encoder == 0U);
    assert_off();
}
static void check_motor(void)
{
    assert(motor_update(100, -200, 1));
    assert_off();
    assert(motor_update(100, -200, 2));
    assert(active_pwm == 0U && (GPIOA->ODR & 0x920U) == 0x920U && (GPIOB->ODR & 8U));
    assert(motor_update(100, -200, 2));
    assert(active_pwm == 0U);
    assert(motor_update(100, -200, 3));
    assert(active_pwm == 15U);
    assert(TIM1->CCR1 == 800U && TIM1->CCR4 == 1000U);
    assert(TIM2->CCR1 == 900U && TIM2->CCR2 == 1000U);
    assert(motor_update(1000, -1000, 4));
    assert(TIM1->CCR1 == 0U && TIM2->CCR1 == 0U);
    assert(motor_update(-300, 300, 5));
    assert_off();
    assert(motor_update(-300, 300, 6));
    assert_off();
    assert(motor_update(-300, 300, 7));
    assert(active_pwm == 0U);
    assert(motor_update(-300, 300, 8));
    assert(TIM1->CCR1 == 1000U && TIM1->CCR4 == 700U);
    assert(TIM2->CCR1 == 1000U && TIM2->CCR2 == 700U);
    assert(!motor_update(1001, 0, 9));
    assert_off();
}
static void check_timeout(void)
{
    assert(wheel_io_request(200, -100, 55));
    step(10);
    assert(status().motor.phase_left == MOTOR_WAKING);
    step(10);
    assert(status().motor.applied_left == 200);
    step(30);
    assert(status().fault);
    assert_off();
    now = UINT32_MAX - 25U;
    assert(wheel_io_init(now));
    assert(wheel_io_request(-200, 100, 55));
    for (unsigned i = 0; i < 5; i++)
    {
        step(10);
    }
    assert(status().remaining_ms == 5U && status().motor.applied_right == 100);
    step(10);
    assert_off();
    assert(status().remaining_ms == 0U && !status().fault);
    for (unsigned i = 0; i < 100; i++)
    {
        step(10);
    }
    assert_off();
}
static void check_stop(void)
{
    assert(wheel_io_request(300, 300, 2000));
    step(10);
    step(10);
    wheel_io_stop();
    assert(!wheel_io_request(300, 300, 2000));
    step(10);
    assert_off();
    assert(status().remaining_ms == 0U);
    assert(wheel_io_request(100, 0, 100));
    step(10);
    wheel_io_stop();
    step(10);
    assert_off();
    assert(wheel_io_request(0, 0, 100));
    step(10);
    assert_off();
}
static void check_fault(void)
{
    assert(wheel_io_request(100, 100, 100));
    step(10);
    fail_start = starts + 2U;
    step(10);
    assert_off();
    assert(status().fault && !wheel_io_request(100, 100, 100));
    assert(wheel_io_init(now));
    assert(wheel_io_request(100, 100, 100));
    step(10);
    step(10);
    now += 10;
    wheel_io_step(now, true);
    assert_off();
    assert(status().fault && !wheel_io_request(100, 100, 100));
}
static void check_commands(void)
{
    char response[1024];
    const char *bad[] = {"motor left 801 100",    "motor right -801 100",
                         "motor left 1 0",        "motor left 1 2001",
                         "motor both 1 2",        "motor left 1 100 extra",
                         "motor left - 100",      "motor both 999999999999999999999999 1 100",
                         "motor both 0x10 0 100", "motor wrong"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
    {
        assert(motor_commands_execute(bad[i], response, sizeof(response)));
        assert(strncmp(response, "ERR", 3) == 0);
    }
    step(10);
    assert_off();
    assert(!motor_commands_execute("wheel left 100 100", response, sizeof(response)));
    assert(!wheel_io_request(801, 0, 100));
    assert(!wheel_io_request(0, -801, 100));
    assert(!wheel_io_request(1, 0, 2001));
    assert(motor_commands_execute("motor both -800 800 2000", response, sizeof(response)));
    assert(strncmp(response, "OK motor queued", 15) == 0);
    step(10);
    step(10);
    assert(status().motor.applied_left == -800);
    assert(motor_commands_execute("motor right 100 100", response, sizeof(response)));
    step(10);
    assert(status().motor.applied_left == 0 && status().motor.applied_right == 100);
    assert(motor_commands_execute("encoder", response, sizeof(response)));
    assert(strstr(response, "left=M2 right=M1") && strstr(response, "counter_left=0"));
    assert(motor_commands_execute("motor stop", response, sizeof(response)));
    step(10);
    assert_off();
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    init();
    if (!strcmp(argv[1], "startup"))
    {
        check_startup();
    }
    else if (!strcmp(argv[1], "configuration"))
    {
        check_configuration();
    }
    else if (!strcmp(argv[1], "encoder"))
    {
        check_encoder();
    }
    else if (!strcmp(argv[1], "motor"))
    {
        check_motor();
    }
    else if (!strcmp(argv[1], "timeout"))
    {
        check_timeout();
    }
    else if (!strcmp(argv[1], "stop"))
    {
        check_stop();
    }
    else if (!strcmp(argv[1], "fault"))
    {
        check_fault();
    }
    else if (!strcmp(argv[1], "commands"))
    {
        check_commands();
    }
    else
    {
        abort();
    }
    assert(critical_depth == 0U);
    printf("PASS %s\n", argv[1]);
    return 0;
}
