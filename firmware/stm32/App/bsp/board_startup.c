#include "board_startup.h"

#include "main.h"

void board_startup_hold_outputs(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* C/D 由电机模块接管；这里只保持未启用的舵机和 A/B 通道为低电平。 */
    gpio.Pin = Servo1_Pin | Servo2_Pin | Servo3_Pin | Servo4_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(GPIOE, gpio.Pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOE, &gpio);

    gpio.Pin = PWMA_Pin | PWMB_Pin;
    HAL_GPIO_WritePin(GPIOE, gpio.Pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOE, &gpio);
}
