#ifndef ROBOT_WHEEL_CONFIG_H
#define ROBOT_WHEEL_CONFIG_H

/* 沿用原控制周期；阶段 2 手动开环测试限制为 30%，单次最长 2 秒。 */
#define WHEEL_PERIOD_MS 10U
#define WHEEL_MANUAL_MAX_DUTY_PERMILLE 300
#define WHEEL_MANUAL_MAX_DURATION_MS 2000U

#endif
