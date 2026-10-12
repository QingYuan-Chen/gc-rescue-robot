#ifndef ROBOT_BOARD_STARTUP_H
#define ROBOT_BOARD_STARTUP_H

/* GPIO 时钟已启用；在外设模块接管之前，将执行器信号保持为低电平。 */
void board_startup_hold_outputs(void);

#endif
