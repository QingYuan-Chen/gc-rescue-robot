#include "wheel_drive.h"

#include "bsp/encoder.h"
#include "bsp/motor.h"
#include "config/wheel_config.h"
#include "wheel_pid.h"

#include "FreeRTOS.h"
#include "task.h"

_Static_assert(configTICK_RATE_HZ == 1000U, "车轮任务的 tick 与毫秒必须一一对应");

typedef struct
{
    wheel_drive_mode_t mode;
    bool enabled[WHEEL_COUNT];
    int32_t duty_permille[WHEEL_COUNT];
    int32_t target_milli_counts[WHEEL_COUNT];
    uint32_t duration_ms;
} wheel_request_t;

/* 请求与停车标志分别交接，运动请求不能覆盖尚未执行的停车。 */
static wheel_request_t pending_request;
static bool request_pending;
static bool stop_pending;
static bool ready;
static bool period_fault;
static wheel_request_result_t last_reject_reason;
static wheel_drive_status_t snapshot;

/* 以下状态只由车轮任务修改；PID、期限和换向使用同一周期推进。 */
static wheel_request_t active_request;
static uint32_t started_tick_ms;
static pid_incremental_state_t pid[WHEEL_COUNT];
static wheel_drive_status_t current;

static void reset_controllers(void)
{
    for (wheel_id_t wheel = WHEEL_LEFT; wheel < WHEEL_COUNT; wheel++)
    {
        pid_incremental_reset(&pid[wheel]);
    }
}

bool wheel_drive_init(void)
{
    pending_request = (wheel_request_t){0};
    active_request = (wheel_request_t){0};
    current = (wheel_drive_status_t){0};
    snapshot = current;
    request_pending = false;
    stop_pending = false;
    ready = false;
    period_fault = false;
    last_reject_reason = WHEEL_REQUEST_ACCEPTED;
    started_tick_ms = 0U;
    reset_controllers();
    return motor_init() && encoder_init();
}

static bool valid_duration(uint32_t duration_ms)
{
    return duration_ms >= WHEEL_PERIOD_MS && duration_ms <= WHEEL_MANUAL_MAX_DURATION_MS &&
           duration_ms % WHEEL_PERIOD_MS == 0U;
}

static wheel_request_result_t reject_invalid_request(void)
{
    taskENTER_CRITICAL();
    last_reject_reason = WHEEL_REQUEST_INVALID;
    taskEXIT_CRITICAL();
    return WHEEL_REQUEST_INVALID;
}

static wheel_request_result_t queue_request(const wheel_request_t *request)
{
    wheel_request_result_t result = WHEEL_REQUEST_ACCEPTED;
    taskENTER_CRITICAL();
    if (!ready)
    {
        result = WHEEL_REQUEST_NOT_READY;
    }
    else if (period_fault)
    {
        result = WHEEL_REQUEST_PERIOD_FAULT;
    }
    else if (stop_pending)
    {
        result = WHEEL_REQUEST_STOP_PENDING;
    }
    else
    {
        pending_request = *request;
        request_pending = true;
    }
    if (result != WHEEL_REQUEST_ACCEPTED)
    {
        last_reject_reason = result;
    }
    taskEXIT_CRITICAL();
    return result;
}

wheel_request_result_t wheel_drive_request_open_loop(int32_t duty_left, int32_t duty_right,
                                                     uint32_t duration_ms)
{
    if (duty_left < -WHEEL_MANUAL_MAX_DUTY_PERMILLE || duty_left > WHEEL_MANUAL_MAX_DUTY_PERMILLE ||
        duty_right < -WHEEL_MANUAL_MAX_DUTY_PERMILLE ||
        duty_right > WHEEL_MANUAL_MAX_DUTY_PERMILLE || !valid_duration(duration_ms))
    {
        return reject_invalid_request();
    }
    const wheel_request_t request = {
        .mode = WHEEL_MODE_OPEN_LOOP,
        .enabled = {true, true},
        .duty_permille = {duty_left, duty_right},
        .duration_ms = duration_ms,
    };
    return queue_request(&request);
}

wheel_request_result_t wheel_drive_request_speed(wheel_drive_selection_t selection,
                                                 int32_t left_milli_counts,
                                                 int32_t right_milli_counts, uint32_t duration_ms)
{
    if ((unsigned)selection > WHEEL_SELECT_RIGHT ||
        left_milli_counts < -WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS ||
        left_milli_counts > WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS ||
        right_milli_counts < -WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS ||
        right_milli_counts > WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS ||
        (selection == WHEEL_SELECT_LEFT && right_milli_counts != 0) ||
        (selection == WHEEL_SELECT_RIGHT && left_milli_counts != 0) || !valid_duration(duration_ms))
    {
        return reject_invalid_request();
    }
    const wheel_request_t request = {
        .mode = WHEEL_MODE_SPEED_LOOP,
        .enabled = {selection != WHEEL_SELECT_RIGHT, selection != WHEEL_SELECT_LEFT},
        .target_milli_counts = {left_milli_counts, right_milli_counts},
        .duration_ms = duration_ms,
    };
    return queue_request(&request);
}

void wheel_drive_stop(void)
{
    taskENTER_CRITICAL();
    stop_pending = true;
    request_pending = false;
    taskEXIT_CRITICAL();
}

void wheel_drive_get_status(wheel_drive_status_t *status)
{
    taskENTER_CRITICAL();
    *status = snapshot;
    status->last_reject_reason = last_reject_reason;
    taskEXIT_CRITICAL();
}

static void enter_stop(wheel_stop_reason_t reason)
{
    active_request = (wheel_request_t){0};
    reset_controllers();
    current.mode = WHEEL_MODE_STOP;
    current.last_stop_reason = reason;
    current.remaining_ms = 0U;
    current.transitioning = false;
}

static void adopt_request(const wheel_request_t *request, uint32_t tick_ms)
{
    current.transitioning = current.mode != WHEEL_MODE_STOP && current.mode != request->mode;
    if (current.mode != request->mode)
    {
        reset_controllers();
    }
    else
    {
        for (wheel_id_t wheel = WHEEL_LEFT; wheel < WHEEL_COUNT; wheel++)
        {
            if (active_request.enabled[wheel] != request->enabled[wheel])
            {
                /* 单轮验收换侧时清理被停用/新启用的实例，连续启用侧保留历史。 */
                pid_incremental_reset(&pid[wheel]);
            }
        }
    }
    active_request = *request;
    current.mode = request->mode;
    started_tick_ms = tick_ms;
}

static void update_outputs(void)
{
    for (wheel_id_t wheel = WHEEL_LEFT; wheel < WHEEL_COUNT; wheel++)
    {
        current.enabled[wheel] = active_request.enabled[wheel];
        current.target_milli_counts[wheel] = active_request.target_milli_counts[wheel];
        current.limited[wheel] = false;
        current.reversing[wheel] = false;
        int32_t requested = 0;
        if (!current.transitioning && current.enabled[wheel])
        {
            if (current.mode == WHEEL_MODE_SPEED_LOOP)
            {
                requested = wheel_pid_step_duty_permille(
                    &pid[wheel], (float)current.target_milli_counts[wheel] / 1000.0f,
                    current.delta_counts[wheel]);
            }
            else if (current.mode == WHEEL_MODE_OPEN_LOOP)
            {
                requested = active_request.duty_permille[wheel];
            }
        }
        current.pid_output_counts[wheel] = pid[wheel].output;
        current.requested_duty_permille[wheel] = requested;
        int32_t applied = requested;
        if (current.mode == WHEEL_MODE_SPEED_LOOP)
        {
            if (applied > WHEEL_SPEED_MAX_DUTY_PERMILLE)
            {
                applied = WHEEL_SPEED_MAX_DUTY_PERMILLE;
            }
            else if (applied < -WHEEL_SPEED_MAX_DUTY_PERMILLE)
            {
                applied = -WHEEL_SPEED_MAX_DUTY_PERMILLE;
            }
            current.limited[wheel] = applied != requested;
        }
        if (applied * current.duty_permille[wheel] < 0)
        {
            current.reversing[wheel] = true;
            applied = 0;
        }
        /* 限幅与换向只作用于实际输出，不反写 PID 的累计输出。 */
        motor_set_duty_permille(wheel, applied);
        current.duty_permille[wheel] = applied;
    }
}

void wheel_drive_step(uint32_t tick_ms, bool schedule_error)
{
    current.interval_ms = current.ready ? tick_ms - current.sample_tick_ms : 0U;
    current.sample_tick_ms = tick_ms;
    for (wheel_id_t wheel = WHEEL_LEFT; wheel < WHEEL_COUNT; wheel++)
    {
        current.delta_counts[wheel] = encoder_read_delta_counts(wheel, &current.raw_counts[wheel]);
        current.total_counts[wheel] += current.delta_counts[wheel];
    }

    if (current.ready)
    {
        wheel_request_t request;
        taskENTER_CRITICAL();
        period_fault = period_fault || schedule_error || current.interval_ms != WHEEL_PERIOD_MS;
        const bool have_request = request_pending;
        const bool have_stop = stop_pending;
        request = pending_request;
        request_pending = false;
        stop_pending = false;
        current.period_fault = period_fault;
        taskEXIT_CRITICAL();

        current.transitioning = false;
        if (current.period_fault)
        {
            enter_stop(WHEEL_STOP_PERIOD_ERROR);
        }
        else if (have_stop)
        {
            enter_stop(WHEEL_STOP_COMMAND);
        }
        else
        {
            if (have_request)
            {
                adopt_request(&request, tick_ms);
            }
            if (current.mode != WHEEL_MODE_STOP)
            {
                const uint32_t elapsed_ms = tick_ms - started_tick_ms;
                if (elapsed_ms >= active_request.duration_ms)
                {
                    enter_stop(WHEEL_STOP_EXPIRED);
                }
                else
                {
                    current.remaining_ms = active_request.duration_ms - elapsed_ms;
                }
            }
        }
    }
    /* 第一次同步采样只建立时间基线，尚未接收任何运动请求。 */
    current.ready = true;
    update_outputs();
    taskENTER_CRITICAL();
    snapshot = current;
    ready = true;
    taskEXIT_CRITICAL();
}
