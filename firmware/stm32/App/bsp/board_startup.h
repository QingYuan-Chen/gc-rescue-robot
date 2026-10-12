#ifndef ROBOT_BOARD_STARTUP_H
#define ROBOT_BOARD_STARTUP_H

/* GPIO 时钟已启用；只保持尚未启用的 C/D 和舵机信号为低电平。 */
void board_startup_hold_outputs(void);

#endif
