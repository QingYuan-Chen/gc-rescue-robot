#include "board_outputs.h"

#include "main.h"

#define MOTOR_A_PINS (M1_IN1_Pin | M1_IN2_Pin | M2_IN1_Pin)
#define MOTOR_B_PINS (M2_IN2_Pin | M3_IN1_Pin | M3_IN2_Pin | M4_IN1_Pin | M4_IN2_Pin)
#define SERVO_PINS (Servo1_Pin | Servo2_Pin | Servo3_Pin | Servo4_Pin)

static void hold_low(GPIO_TypeDef *port, uint32_t pins)
{
    GPIO_InitTypeDef gpio = {0};
    /* 先清输出锁存，再切换 GPIO 模式，避免输出旧的高电平。 */
    HAL_GPIO_WritePin(port, pins, GPIO_PIN_RESET);
    gpio.Pin = pins;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &gpio);
}

void board_outputs_disable(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* TB67H450 的 IN1/IN2 同为低电平时输出高阻，持续保持即进入待机。 */
    hold_low(GPIOA, MOTOR_A_PINS);
    hold_low(GPIOB, MOTOR_B_PINS);
    hold_low(GPIOE, SERVO_PINS);
}

static bool pins_are_low_outputs(const GPIO_TypeDef *port, uint32_t pins)
{
    if ((port->ODR & pins) != 0U)
    {
        return false;
    }
    for (uint32_t pin = 0U; pin < 16U; pin++)
    {
        if ((pins & (1UL << pin)) != 0U && ((port->MODER >> (pin * 2U)) & 3U) != 1U)
        {
            return false;
        }
    }
    return true;
}

bool board_outputs_are_disabled(void)
{
    return pins_are_low_outputs(GPIOA, MOTOR_A_PINS) &&
           pins_are_low_outputs(GPIOB, MOTOR_B_PINS) &&
           pins_are_low_outputs(GPIOE, SERVO_PINS);
}
