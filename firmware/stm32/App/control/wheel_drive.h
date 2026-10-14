#ifndef ROBOT_WHEEL_DRIVE_H
#define ROBOT_WHEEL_DRIVE_H

#include "bsp/wheel_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    WHEEL_MODE_STOP = 0,
    WHEEL_MODE_OPEN_LOOP,
    WHEEL_MODE_SPEED_LOOP
} wheel_drive_mode_t;

typedef enum
{
    WHEEL_STOP_BOOT = 0,
    WHEEL_STOP_COMMAND,
    WHEEL_STOP_EXPIRED,
    WHEEL_STOP_PERIOD_ERROR
} wheel_stop_reason_t;

typedef enum
{
    WHEEL_REQUEST_ACCEPTED = 0,
    WHEEL_REQUEST_INVALID,
    WHEEL_REQUEST_NOT_READY,
    WHEEL_REQUEST_STOP_PENDING,
    WHEEL_REQUEST_PERIOD_FAULT
} wheel_request_result_t;

typedef enum
{
    WHEEL_SELECT_BOTH = 0,
    WHEEL_SELECT_LEFT,
    WHEEL_SELECT_RIGHT
} wheel_drive_selection_t;

typedef struct
{
    uint32_t sample_tick_ms;
    uint32_t interval_ms;
    uint32_t remaining_ms;
    wheel_drive_mode_t mode;
    wheel_stop_reason_t last_stop_reason;
    wheel_request_result_t last_reject_reason;
    bool ready;
    bool period_fault;
    bool transitioning;
    bool enabled[WHEEL_COUNT];
    bool limited[WHEEL_COUNT];
    bool reversing[WHEEL_COUNT];
    int32_t target_milli_counts[WHEEL_COUNT];
    float pid_output_counts[WHEEL_COUNT];
    int32_t requested_duty_permille[WHEEL_COUNT];
    int32_t duty_permille[WHEEL_COUNT];
    int32_t delta_counts[WHEEL_COUNT];
    int64_t total_counts[WHEEL_COUNT];
    uint32_t raw_counts[WHEEL_COUNT];
} wheel_drive_status_t;

bool wheel_drive_init(void);
/* 初始化后立即调用一次建立采样基线，此后仅由唯一 10 ms 任务调用。
 * 错过调度或实际间隔异常时停车并禁止运动，排查后复位才能解除。
 */
void wheel_drive_step(uint32_t tick_ms, bool schedule_error);

/* 以下是限时调试入口，均按左、右排列；同周期停止优先于任何运动请求。
 * 同模式续期不清 PID。请求时长从控制任务采纳时开始，包含过渡及换向间隔。
 */
wheel_request_result_t wheel_drive_request_open_loop(int32_t duty_left, int32_t duty_right,
                                                     uint32_t duration_ms);
/* 目标单位为千分之一计数/10 ms；未选择侧必须传 0，并明确停用其控制器。
 * 已启用轮子的零目标仍运行 PID，不等同于下面的显式停车。
 */
wheel_request_result_t wheel_drive_request_speed(wheel_drive_selection_t selection,
                                                 int32_t left_milli_counts,
                                                 int32_t right_milli_counts, uint32_t duration_ms);
void wheel_drive_stop(void);
void wheel_drive_get_status(wheel_drive_status_t *status);

#endif
