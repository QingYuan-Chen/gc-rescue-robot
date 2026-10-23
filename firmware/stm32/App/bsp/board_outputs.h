#ifndef ROBOT_BOARD_OUTPUTS_H
#define ROBOT_BOARD_OUTPUTS_H

#include <stdbool.h>

/* 复位入口和故障入口均可调用，不依赖 HAL 时基、RTOS 或定时器句柄。 */
void board_outputs_disable(void);

/* 核对全部四路驱动输入和舵机信号的实际 GPIO 模式与输出锁存值。 */
bool board_outputs_are_disabled(void);

#endif
