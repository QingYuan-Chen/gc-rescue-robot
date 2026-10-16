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
    uint32_t duration_ms;
} wheel_request_t;

typedef struct
{
    uint32_t kp_milli;
    uint32_t ki_milli;
    uint32_t id;
} wheel_pi_request_t;

/* 请求与停车标志分别交接，运动请求不能覆盖尚未执行的停车。 */
static wheel_request_t pending_request;
static bool request_pending;
static bool stop_pending;
static bool ready;
static bool period_fault;
static wheel_request_result_t last_reject_reason;
static wheel_drive_status_t snapshot;
static wheel_pi_request_t pending_pi;
static bool pi_pending;
static uint32_t next_pi_request_id;
static int32_t configured_target_milli_counts[WHEEL_COUNT];

/* 以下状态只由车轮任务修改；PI、开环期限和换向使用同一周期推进。 */
static wheel_request_t active_request;
static uint32_t started_tick_ms;
static wheel_pid_t pid[WHEEL_COUNT];
static wheel_drive_status_t current;

static void reset_controllers(void)
{
    for (wheel_id_t wheel = WHEEL_LEFT; wheel < WHEEL_COUNT; wheel++)
    {
        wheel_pid_reset(&pid[wheel]);
    }
}

bool wheel_drive_init(const pid_pi_params_t params[WHEEL_COUNT])
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
    pending_pi = (wheel_pi_request_t){0};
    pi_pending = false;
    next_pi_request_id = 0U;
    for (wheel_id_t wheel = WHEEL_LEFT; wheel < WHEEL_COUNT; wheel++)
    {
        configured_target_milli_counts[wheel] = 0;
        wheel_pid_init(&pid[wheel], &params[wheel]);
    }
    return motor_init() && encoder_init();
}

static bool valid_duration(uint32_t duration_ms)
{
    return duration_ms >= WHEEL_PERIOD_MS && duration_ms <= WHEEL_OPEN_LOOP_MAX_DURATION_MS &&
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

wheel_request_result_t wheel_drive_set_speed_target(wheel_drive_selection_t selection,
                                                    int32_t left_milli_counts,
                                                    int32_t right_milli_counts)
{
    if ((unsigned)selection > WHEEL_SELECT_RIGHT ||
        left_milli_counts < -WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS ||
        left_milli_counts > WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS ||
        right_milli_counts < -WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS ||
        right_milli_counts > WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS ||
        (selection == WHEEL_SELECT_LEFT && right_milli_counts != 0) ||
        (selection == WHEEL_SELECT_RIGHT && left_milli_counts != 0))
    {
        return reject_invalid_request();
    }
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
    else
    {
        if (selection != WHEEL_SELECT_RIGHT)
        {
            configured_target_milli_counts[WHEEL_LEFT] = left_milli_counts;
        }
        if (selection != WHEEL_SELECT_LEFT)
        {
            configured_target_milli_counts[WHEEL_RIGHT] = right_milli_counts;
        }
    }
    if (result != WHEEL_REQUEST_ACCEPTED)
    {
        last_reject_reason = result;
    }
    taskEXIT_CRITICAL();
    return result;
}

wheel_request_result_t wheel_drive_start_speed(wheel_drive_selection_t selection)
{
    if ((unsigned)selection > WHEEL_SELECT_RIGHT)
    {
        return reject_invalid_request();
    }
    const wheel_request_t request = {
        .mode = WHEEL_MODE_SPEED_LOOP,
        .enabled = {selection != WHEEL_SELECT_RIGHT, selection != WHEEL_SELECT_LEFT},
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

wheel_request_result_t wheel_drive_request_left_pi(uint32_t kp_milli, uint32_t ki_milli,
                                                   uint32_t *request_id)
{
    if (kp_milli > WHEEL_PI_MAX_GAIN_MILLI || ki_milli > WHEEL_PI_MAX_GAIN_MILLI)
    {
        return WHEEL_REQUEST_INVALID;
    }
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
    else if (pi_pending)
    {
        result = WHEEL_REQUEST_BUSY;
    }
    else
    {
        pending_pi = (wheel_pi_request_t){kp_milli, ki_milli, ++next_pi_request_id};
        pi_pending = true;
        *request_id = pending_pi.id;
    }
    taskEXIT_CRITICAL();
    return result;
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
                wheel_pid_reset(&pid[wheel]);
            }
        }
    }
    active_request = *request;
    current.mode = request->mode;
    current.remaining_ms = request->duration_ms;
    started_tick_ms = tick_ms;
}

static void update_outputs(void)
{
    for (wheel_id_t wheel = WHEEL_LEFT; wheel < WHEEL_COUNT; wheel++)
    {
        current.enabled[wheel] = active_request.enabled[wheel];
        current.target_milli_counts[wheel] =
            current.mode == WHEEL_MODE_SPEED_LOOP && current.enabled[wheel]
                ? current.configured_target_milli_counts[wheel]
                : 0;
        current.limited[wheel] = false;
        current.reversing[wheel] = false;
        float requested = 0.0f;
        const bool run_pi = !current.transitioning && current.enabled[wheel] &&
                            current.mode == WHEEL_MODE_SPEED_LOOP;
        if (!current.transitioning && current.enabled[wheel])
        {
            if (current.mode == WHEEL_MODE_SPEED_LOOP)
            {
                requested = wheel_pid_prepare(&pid[wheel],
                                              (float)current.target_milli_counts[wheel] / 1000.0f,
                                              current.delta_counts[wheel]);
            }
            else if (current.mode == WHEEL_MODE_OPEN_LOOP)
            {
                requested = (float)active_request.duty_permille[wheel];
            }
        }
        float bounded = requested;
        if (current.mode == WHEEL_MODE_SPEED_LOOP)
        {
            if (bounded > WHEEL_SPEED_MAX_DUTY_PERMILLE)
            {
                bounded = WHEEL_SPEED_MAX_DUTY_PERMILLE;
            }
            else if (bounded < -WHEEL_SPEED_MAX_DUTY_PERMILLE)
            {
                bounded = -WHEEL_SPEED_MAX_DUTY_PERMILLE;
            }
            current.limited[wheel] = bounded != requested;
        }
        /* 浮点输出先限幅再转整数；普通 PWM 量化不触发抗饱和回算。 */
        int32_t applied = (int32_t)bounded;
        current.requested_duty_permille[wheel] = applied;
        if (applied * current.duty_permille[wheel] < 0)
        {
            current.reversing[wheel] = true;
            applied = 0;
        }
        motor_set_duty_permille(wheel, applied);
        current.duty_permille[wheel] = applied;
        if (run_pi)
        {
            wheel_pid_track(&pid[wheel], applied,
                            current.limited[wheel] || current.reversing[wheel]);
        }
        wheel_pid_get_status(&pid[wheel], &current.pi[wheel]);
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
        wheel_pi_request_t pi_request;
        taskENTER_CRITICAL();
        period_fault = period_fault || schedule_error || current.interval_ms != WHEEL_PERIOD_MS;
        const bool have_request = request_pending;
        const bool have_stop = stop_pending;
        request = pending_request;
        const bool have_pi = pi_pending;
        pi_request = pending_pi;
        pi_pending = false;
        request_pending = false;
        stop_pending = false;
        current.period_fault = period_fault;
        for (wheel_id_t wheel = WHEEL_LEFT; wheel < WHEEL_COUNT; wheel++)
        {
            current.configured_target_milli_counts[wheel] = configured_target_milli_counts[wheel];
        }
        taskEXIT_CRITICAL();

        if (have_pi)
        {
            current.left_pi_request_id = pi_request.id;
            current.left_pi_update_tick_ms = tick_ms;
            current.left_pi_update_result =
                current.period_fault ? WHEEL_PI_UPDATE_PERIOD_FAULT : WHEEL_PI_UPDATE_APPLIED;
            if (!current.period_fault)
            {
                /* 仅改参时使用双精度缩放，避免 Ofast 的 float 倒数近似改变末位回读。 */
                wheel_pid_set_gains(&pid[WHEEL_LEFT], (float)((double)pi_request.kp_milli / 1000.0),
                                    (float)((double)pi_request.ki_milli / 1000.0));
            }
        }

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
            if (current.mode == WHEEL_MODE_OPEN_LOOP)
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
