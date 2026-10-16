#ifndef ROBOT_WHEEL_PID_CONFIG_H
#define ROBOT_WHEEL_PID_CONFIG_H

#include "bsp/wheel_types.h"
#include "control/pid.h"

/* 新 PI 尚未上板整定，上电默认 Kp/Ki 均为零，未在线改参时速度命令无输出。
 * 后续由用户在左轮 C 亲自调参；右轮独立保留，不自动复制左轮参数。
 * 回算时间常数暂取 0.1 s（增益 10/s），只作为实现初值，不代表硬件验证结果。
 * 原 Kp=1600、Ki=180 的 CCR 参数仅保留在迁移文档和 Git 历史中。
 */
static const pid_pi_params_t wheel_pid_defaults[WHEEL_COUNT] = {
    [WHEEL_LEFT] = {.kp = 0.0f, .ki = 0.0f, .tracking_gain_per_s = 10.0f},
    [WHEEL_RIGHT] = {.kp = 0.0f, .ki = 0.0f, .tracking_gain_per_s = 10.0f},
};

#endif
