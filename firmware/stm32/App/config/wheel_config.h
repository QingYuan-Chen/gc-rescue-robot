#ifndef ROBOT_WHEEL_CONFIG_H
#define ROBOT_WHEEL_CONFIG_H

/* 当前时钟树的 APB1/APB2 定时器时钟均为 240 MHz：240 MHz / 12 / 1000 = 20 kHz。 */
#define WHEEL_PWM_PRESCALER 11U
#define WHEEL_PWM_PERIOD 1000U
#define WHEEL_SAMPLE_PERIOD_MS 10U
#define WHEEL_SPEED_SAMPLE_PERIOD_MS 50U
/* 测速低通时间常数，初始调试值，需结合实车响应整定。 */
#define WHEEL_SPEED_FILTER_TAU_US 80000.0f
/* 当前最大 PWM 条件下的实测速度，左右轮统一使用，单位 RPM。 */
#define WHEEL_TARGET_MIN_RPM (-81.0f)
#define WHEEL_TARGET_MAX_RPM 97.0f
#define WHEEL_ENCODER_FILTER 10U

/* 根据调试反馈暂定的速度换算系数，实际转速仍需独立测量校验。 */
#define WHEEL_ENCODER_COUNTS_PER_REV 1980.0f

/* 初次联调仅允许限时开环；不是电流或转矩限制，也不是已验收的工作参数。 */
#define WHEEL_COMMAND_MAX_DUTY 800
#define WHEEL_COMMAND_MAX_DURATION_MS 2000U

/* 满足 TB67H450 的待机进入上限 1.5 ms 和退出上限 30 us；由周期任务推进。 */
#define WHEEL_STANDBY_MIN_MS 2U
#define WHEEL_WAKE_MIN_MS 1U

#endif
