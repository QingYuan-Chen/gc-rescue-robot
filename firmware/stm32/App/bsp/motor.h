#ifndef ROBOT_MOTOR_H
#define ROBOT_MOTOR_H

#include "wheel_types.h"

#include <stdbool.h>
#include <stdint.h>

/* 定时器初始化后调用；以零输出启动 A/B，不启动其他执行器。 */
bool motor_init(void);

/* 仅车轮周期任务调用，范围 -1000..1000，零值同时清除 PWM 与方向输出。
 * 正值约定为车体前进，物理方向由 wheel_board.h 配置并经实车确认。
 */
void motor_set_duty_permille(wheel_id_t wheel, int32_t duty_permille);

#endif
