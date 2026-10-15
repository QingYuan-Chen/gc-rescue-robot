#ifndef ROBOT_WHEEL_CONFIG_H
#define ROBOT_WHEEL_CONFIG_H

/* 沿用原控制周期；调试请求限时，不作为后续比赛动作的统一时限。 */
#define WHEEL_PERIOD_MS 10U
#define WHEEL_MANUAL_MAX_DUTY_PERMILLE 300
#define WHEEL_MANUAL_MAX_DURATION_MS 2000U

/* 闭环输出保持在已测过的 25% 范围内，最终约束结果反馈给 PI 回算。
 * 目标单位为千分之一计数/10 ms；±10 计数覆盖此前 ±250‰ 的观测量级。
 * 这只是调试输入范围，不代表闭环可达速度或完整工作范围已经通过验收。
 */
#define WHEEL_SPEED_MAX_DUTY_PERMILLE 250
#define WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS 10000

#endif
