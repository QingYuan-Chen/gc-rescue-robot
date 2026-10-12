#include "board_startup.h"

#include "main.h"

void board_startup_hold_outputs(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* 方向脚已由 MX_GPIO_Init 清零；PWM 引脚也主动保持低电平，避免悬空。
     * 后续电机和舵机模块初始化时，才将各自引脚切换到定时器复用功能。
     */
    gpio.Pin = PWMA_Pin | PWMB_Pin | Servo1_Pin | Servo2_Pin | Servo3_Pin | Servo4_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(GPIOE, gpio.Pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOE, &gpio);

    gpio.Pin = PWMC_Pin | PWMD_Pin;
    HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_RESET);
    HAL_GPIO_Init(GPIOB, &gpio);
}
