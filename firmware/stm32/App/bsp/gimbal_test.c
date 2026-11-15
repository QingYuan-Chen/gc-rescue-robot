#include "gimbal_test.h"
#include "main.h"
#include <stdbool.h>

/* 50 Hz，按键控制夹爪打开和闭合。 */
#define GRIPPER_OPEN_US 1750U
#define GRIPPER_CLOSED_US 2300U
#define GIMBAL_PERIOD_US 20000U
#define KEY_DEBOUNCE_SAMPLES 3U

static volatile uint32_t pulse_us = GRIPPER_OPEN_US;
static volatile uint32_t active_pulse_us;
static volatile bool high_phase;
static bool running;
static uint8_t stable_keys;
static uint8_t last_sample;
static uint8_t sample_count;

void gimbal_test_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &gpio);

    __HAL_RCC_TIM7_CLK_ENABLE();
    TIM7->CR1 = 0U;
    TIM7->DIER = 0U;
    TIM7->PSC = 239U;
    TIM7->ARR = GIMBAL_PERIOD_US - 1U;
    TIM7->EGR = TIM_EGR_UG;
    TIM7->SR = 0U;
    HAL_NVIC_SetPriority(TIM7_IRQn, 6U, 0U);
    HAL_NVIC_ClearPendingIRQ(TIM7_IRQn);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);
}

void gimbal_test_step(void)
{
    const uint8_t keys = (uint8_t)((~GPIOC->IDR) & (GPIO_PIN_2 | GPIO_PIN_1));
    if (keys != last_sample)
    {
        last_sample = keys;
        sample_count = 1U;
        return;
    }
    if (sample_count < KEY_DEBOUNCE_SAMPLES) { sample_count++; }
    if (sample_count != KEY_DEBOUNCE_SAMPLES || keys == stable_keys) { return; }
    const uint8_t pressed = (uint8_t)(keys & ~stable_keys);
    stable_keys = keys;
    if (keys == GPIO_PIN_2 && pressed == GPIO_PIN_2)
    {
        pulse_us = GRIPPER_CLOSED_US;
    }
    else if (keys == GPIO_PIN_1 && pressed == GPIO_PIN_1)
    {
        pulse_us = GRIPPER_OPEN_US;
    }
    else { return; }

    if (!running)
    {
        running = true;
        TIM7->CNT = 0U;
        TIM7->ARR = 1U;
        TIM7->EGR = TIM_EGR_UG;
        TIM7->SR = 0U;
        TIM7->DIER = TIM_DIER_UIE;
        TIM7->CR1 = TIM_CR1_CEN;
    }
}

void TIM7_IRQHandler(void)
{
    if ((TIM7->SR & TIM_SR_UIF) == 0U) { return; }
    TIM7->SR = 0U;
    if (!high_phase)
    {
        active_pulse_us = pulse_us;
        GPIOE->BSRR = Servo2_Pin;
        TIM7->ARR = active_pulse_us - 1U;
        high_phase = true;
    }
    else
    {
        GPIOE->BSRR = (uint32_t)Servo2_Pin << 16U;
        TIM7->ARR = GIMBAL_PERIOD_US - active_pulse_us - 1U;
        high_phase = false;
    }
}
