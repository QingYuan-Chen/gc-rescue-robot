#include "tim.h"

#include "config/wheel_config.h"

#include <stdbool.h>

/* M2 PWM 使用 TIM2，M1 PWM 使用 TIM1；编码器使用 TIM5 与 TIM3。 */
TIM_HandleTypeDef htim1, htim2, htim3, htim5;

static void pwm_init(TIM_HandleTypeDef *timer, TIM_TypeDef *instance, uint32_t channel)
{
    TIM_OC_InitTypeDef output = {0};
    timer->Instance = instance;
    timer->Init.Prescaler = WHEEL_PWM_PRESCALER;
    timer->Init.CounterMode = TIM_COUNTERMODE_UP;
    timer->Init.Period = WHEEL_PWM_PERIOD - 1U;
    timer->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(timer) != HAL_OK) { Error_Handler(); }
    output.OCMode = TIM_OCMODE_PWM1;
    output.OCPolarity = TIM_OCPOLARITY_HIGH;
    output.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    output.OCFastMode = TIM_OCFAST_DISABLE;
    output.OCIdleState = TIM_OCIDLESTATE_RESET;
    output.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(timer, &output, TIM_CHANNEL_1) != HAL_OK ||
        HAL_TIM_PWM_ConfigChannel(timer, &output, channel) != HAL_OK) { Error_Handler(); }
}

static void encoder_init(TIM_HandleTypeDef *timer, TIM_TypeDef *instance)
{
    TIM_Encoder_InitTypeDef input = {0};
    timer->Instance = instance;
    timer->Init.Prescaler = 0U;
    timer->Init.CounterMode = TIM_COUNTERMODE_UP;
    timer->Init.Period = 65535U;
    timer->Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    timer->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    /* TI12 用双相边沿计数；RISING 选择输入极性，不是只统计上升沿。 */
    input.EncoderMode = TIM_ENCODERMODE_TI12;
    input.IC1Polarity = TIM_ICPOLARITY_RISING;
    input.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    input.IC1Prescaler = TIM_ICPSC_DIV1;
    input.IC1Filter = WHEEL_ENCODER_FILTER;
    input.IC2Polarity = TIM_ICPOLARITY_RISING;
    input.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    input.IC2Prescaler = TIM_ICPSC_DIV1;
    input.IC2Filter = WHEEL_ENCODER_FILTER;
    if (HAL_TIM_Encoder_Init(timer, &input) != HAL_OK)
    {
        Error_Handler();
    }
}

void MX_TIM1_Init(void) { pwm_init(&htim1, TIM1, TIM_CHANNEL_4); }
void MX_TIM2_Init(void) { pwm_init(&htim2, TIM2, TIM_CHANNEL_2); }
void MX_TIM3_Init(void) { encoder_init(&htim3, TIM3); }
void MX_TIM5_Init(void) { encoder_init(&htim5, TIM5); }

void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *timer)
{
    if (timer->Instance == TIM1) { __HAL_RCC_TIM1_CLK_ENABLE(); }
    else if (timer->Instance == TIM2) { __HAL_RCC_TIM2_CLK_ENABLE(); }
}

void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *timer)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    if (timer->Instance == TIM3)
    {
        __HAL_RCC_TIM3_CLK_ENABLE();
        gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        gpio.Alternate = GPIO_AF2_TIM3;
    }
    else if (timer->Instance == TIM5)
    {
        __HAL_RCC_TIM5_CLK_ENABLE();
        gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1;
        gpio.Alternate = GPIO_AF2_TIM5;
    }
    HAL_GPIO_Init(GPIOA, &gpio);
}
