#include "gimbal_test.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>

/* 50 Hz 时分复用输出双舵机：
 * PC2 切换云台（Servo1 / PE9）1000 μs 俯瞰夹取 / 1300 μs 搜集物料；
 * PC1 切换夹爪（Servo2 / PE11）1750 μs 打开 / 2300 μs 闭合。 */
#define GIMBAL_OVERLOOK_US 1000U
#define GIMBAL_SEARCH_US 1300U
#define GRIPPER_OPEN_US 1750U
#define GRIPPER_CLOSED_US 2300U
#define SERVO_PERIOD_US 20000U
#define KEY_DEBOUNCE_SAMPLES 3U

/* 0 表示该路尚未激活，对应引脚保持低电平。 */
static volatile uint32_t gimbal_pulse_us;
static volatile uint32_t gripper_pulse_us;
static uint32_t frame_gimbal_us;
static uint32_t frame_gripper_us;
static uint8_t phase;
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
    TIM7->ARR = SERVO_PERIOD_US - 1U;
    TIM7->EGR = TIM_EGR_UG;
    TIM7->SR = 0U;
    HAL_NVIC_SetPriority(TIM7_IRQn, 6U, 0U);
    HAL_NVIC_ClearPendingIRQ(TIM7_IRQn);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);

    gimbal_pulse_us = 0U;
    gripper_pulse_us = 0U;
    frame_gimbal_us = 0U;
    frame_gripper_us = 0U;
    phase = 0U;
    running = false;
    stable_keys = 0U;
    last_sample = 0U;
    sample_count = 0U;
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
    if (sample_count < KEY_DEBOUNCE_SAMPLES)
    {
        sample_count++;
    }
    if (sample_count != KEY_DEBOUNCE_SAMPLES || keys == stable_keys)
    {
        return;
    }
    const uint8_t pressed = (uint8_t)(keys & ~stable_keys);
    stable_keys = keys;

    if (keys == GPIO_PIN_2 && pressed == GPIO_PIN_2)
    {
        /* PC2 在云台俯瞰夹取（1000 μs）与搜集物料（1300 μs）之间切换。 */
        gimbal_pulse_us = (gimbal_pulse_us == GIMBAL_OVERLOOK_US) ? GIMBAL_SEARCH_US
                                                                  : GIMBAL_OVERLOOK_US;
    }
    else if (keys == GPIO_PIN_1 && pressed == GPIO_PIN_1)
    {
        /* PC1 在夹爪打开（1750 μs）与闭合（2300 μs）之间切换。 */
        gripper_pulse_us = (gripper_pulse_us == GRIPPER_OPEN_US) ? GRIPPER_CLOSED_US
                                                                 : GRIPPER_OPEN_US;
    }
    else
    {
        return;
    }

    if (!running)
    {
        running = true;
        phase = 0U;
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
    if ((TIM7->SR & TIM_SR_UIF) == 0U)
    {
        return;
    }
    TIM7->SR = 0U;

    if (phase == 0U)
    {
        frame_gimbal_us = gimbal_pulse_us;
        frame_gripper_us = gripper_pulse_us;
        if (frame_gimbal_us != 0U)
        {
            GPIOE->BSRR = Servo1_Pin;
            TIM7->ARR = frame_gimbal_us - 1U;
            phase = 1U;
            return;
        }
        if (frame_gripper_us != 0U)
        {
            GPIOE->BSRR = Servo2_Pin;
            TIM7->ARR = frame_gripper_us - 1U;
            phase = 2U;
            return;
        }
        TIM7->ARR = SERVO_PERIOD_US - 1U;
        return;
    }

    if (phase == 1U)
    {
        GPIOE->BSRR = (uint32_t)Servo1_Pin << 16U;
        if (frame_gripper_us != 0U)
        {
            GPIOE->BSRR = Servo2_Pin;
            TIM7->ARR = frame_gripper_us - 1U;
            phase = 2U;
            return;
        }
        TIM7->ARR = SERVO_PERIOD_US - frame_gimbal_us - 1U;
        phase = 0U;
        return;
    }

    GPIOE->BSRR = (uint32_t)Servo2_Pin << 16U;
    TIM7->ARR = SERVO_PERIOD_US - frame_gimbal_us - frame_gripper_us - 1U;
    phase = 0U;
}
