#ifndef ROBOT_WHEEL_PID_H
#define ROBOT_WHEEL_PID_H

#include "pid.h"

#include <stdint.h>

/* 将原 CCR 尺度输出限幅并截断后，换算为有符号占空比千分比。
 * 不修改 PID 内部输出；相对原整数 CCR 的占空比，新增量化误差最大 0.5 千分比。
 */
int32_t wheel_pid_output_to_duty_permille(float output_counts);

/* 每 10 ms 调用一次；目标和反馈均为该周期计数，前进为正。
 * 初始化或明确清零时调用 pid_incremental_reset；本接口不写电机或启动任务。
 */
int32_t wheel_pid_step_duty_permille(pid_incremental_state_t *state, float target_delta_counts,
                                     int32_t measured_delta_counts);

#endif
