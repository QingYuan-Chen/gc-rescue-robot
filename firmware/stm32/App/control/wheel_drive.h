#ifndef ROBOT_WHEEL_DRIVE_H
#define ROBOT_WHEEL_DRIVE_H

#include "bsp/wheel_types.h"
#include "wheel_pid.h"

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
    WHEEL_REQUEST_PERIOD_FAULT,
    WHEEL_REQUEST_BUSY
} wheel_request_result_t;

typedef enum
{
    WHEEL_PI_UPDATE_NONE = 0,
    WHEEL_PI_UPDATE_APPLIED,
    WHEEL_PI_UPDATE_PERIOD_FAULT
} wheel_pi_update_result_t;

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
    uint32_t left_pi_request_id;
    uint32_t left_pi_update_tick_ms;
    wheel_pi_update_result_t left_pi_update_result;
    bool ready;
    bool period_fault;
    bool transitioning;
    bool enabled[WHEEL_COUNT];
    bool limited[WHEEL_COUNT];
    bool reversing[WHEEL_COUNT];
    int32_t configured_target_milli_counts[WHEEL_COUNT];
    int32_t target_milli_counts[WHEEL_COUNT];
    wheel_pid_status_t pi[WHEEL_COUNT];
    int32_t requested_duty_permille[WHEEL_COUNT];
    int32_t duty_permille[WHEEL_COUNT];
    int32_t delta_counts[WHEEL_COUNT];
    int64_t total_counts[WHEEL_COUNT];
    uint32_t raw_counts[WHEEL_COUNT];
} wheel_drive_status_t;

/* 初始化时复制每轮参数；运行中不从其他任务直接修改控制器内部状态。 */
bool wheel_drive_init(const pid_pi_params_t params[WHEEL_COUNT]);
/* 初始化后立即调用一次建立采样基线，此后仅由唯一 10 ms 任务调用。
 * 错过调度或实际间隔异常时停车并禁止运动，排查后复位才能解除。
 */
void wheel_drive_step(uint32_t tick_ms, bool schedule_error);

/* 开环接线检查采用限时请求，均按左、右排列；同周期停止优先于任何运动请求。
 * 时长从控制任务采纳时开始，包含过渡及换向间隔。
 */
wheel_request_result_t wheel_drive_request_open_loop(int32_t duty_left, int32_t duty_right,
                                                     uint32_t duration_ms);
/* 目标参数单位为千分之一计数/10 ms，未选择侧传 0，其已存参数保持不变。
 * 停车或开环时只保存参数；闭环中下一周期采用新目标，不清积分、不启用其他轮。
 * 普通停车保留参数；复位恢复零目标。参数更新本身不启动电机。
 */
wheel_request_result_t wheel_drive_set_speed_target(wheel_drive_selection_t selection,
                                                    int32_t left_milli_counts,
                                                    int32_t right_milli_counts);
/* 按已保存目标启动所选轮闭环，无到期停车；未选择侧明确停用。
 * 已启用轮的零目标仍运行 PI；停止或周期故障才退出，调参由用户主动停车。
 */
wheel_request_result_t wheel_drive_start_speed(wheel_drive_selection_t selection);
void wheel_drive_stop(void);
void wheel_drive_get_status(wheel_drive_status_t *status);

/* 左轮在线调参；数值以千分之一增益传递，避免文本 NaN/Inf 进入控制层。
 * accepted 仅表示入队；实际结果及参数从周期快照读取，不启动运动。
 */
wheel_request_result_t wheel_drive_request_left_pi(uint32_t kp_milli, uint32_t ki_milli,
                                                   uint32_t *request_id);

#endif
