#include "wheel_io.h"

#include "bsp/time_us.h"
#include "config/wheel_config.h"
#include "main.h"

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "task.h"

#include <math.h>
#include <string.h>



_Static_assert(WHEEL_SPEED_SAMPLE_PERIOD_MS % WHEEL_SAMPLE_PERIOD_MS == 0U,
               "Speed sampling period must be a multiple of the control period");
_Static_assert(WHEEL_SPEED_SAMPLE_PERIOD_MS >= WHEEL_SAMPLE_PERIOD_MS,
               "Speed sampling period must be at least one control period");

typedef struct
{
    int16_t left, right;
    uint32_t issued_ms, duration_ms;
} drive_request_t;

static drive_request_t request;
static bool stop_pending, session_claimed, speed_target_pending;
static uint32_t previous_ms, previous_us;
static uint32_t speed_sample_us, speed_sample_count;
static int32_t speed_counts[2];
static float measured_rpm[2];
static float pending_targets[2];
static wheel_speed_t controllers[2];
static float targets[2];
static float chassis_targets[2];
static bool chassis_claimed;
static wheel_io_status_t published;

bool wheel_io_init(uint32_t now_ms)
{
    request = (drive_request_t){0};
    published = (wheel_io_status_t){0};
    stop_pending = session_claimed = speed_target_pending = false;
    chassis_claimed = false;
    chassis_targets[0] = chassis_targets[1] = 0.0f;
    pending_targets[0] = pending_targets[1] = 0.0f;
    targets[0] = targets[1] = 0.0f;
    for (unsigned i = 0; i < 2U; i++)
    {
        wheel_speed_reset(&controllers[i]);
    }
    previous_ms = now_ms;
    previous_us = time_us_now();
    motor_init(now_ms);
    if (!encoder_init()) { return false; }
    speed_sample_us = time_us_now();
    speed_sample_count = 0U;
    speed_counts[0] = speed_counts[1] = 0;
    measured_rpm[0] = measured_rpm[1] = 0.0f;
    published.ready = true;
    return true;
}

bool wheel_io_request(int16_t left, int16_t right, uint32_t duration_ms)
{
    if (left < -WHEEL_COMMAND_MAX_DUTY || left > WHEEL_COMMAND_MAX_DUTY ||
        right < -WHEEL_COMMAND_MAX_DUTY || right > WHEEL_COMMAND_MAX_DUTY ||
        duration_ms < WHEEL_SAMPLE_PERIOD_MS || duration_ms > WHEEL_COMMAND_MAX_DURATION_MS)
    {
        return false;
    }
    taskENTER_CRITICAL();
    const bool accepted = published.ready && !published.fault && !motor_guard_fault() &&
                          !stop_pending && !session_claimed && !chassis_claimed;
    if (accepted) { request = (drive_request_t){left, right, osKernelGetTickCount(), duration_ms}; }
    taskEXIT_CRITICAL();
    return accepted;
}

void wheel_io_stop(void)
{
    taskENTER_CRITICAL();
    request = (drive_request_t){0};
    stop_pending = true;
    chassis_claimed = false;
    chassis_targets[0] = chassis_targets[1] = 0.0f;
    speed_target_pending = false;
    taskEXIT_CRITICAL();
}

bool wheel_io_speed_target(float left_rpm, float right_rpm)
{
    if (!isfinite(left_rpm) || !isfinite(right_rpm) ||
        left_rpm < WHEEL_TARGET_MIN_RPM || left_rpm > WHEEL_TARGET_MAX_RPM ||
        right_rpm < WHEEL_TARGET_MIN_RPM || right_rpm > WHEEL_TARGET_MAX_RPM)
    {
        return false;
    }
    taskENTER_CRITICAL();
    bool accepted = published.ready && !published.fault && !motor_guard_fault() &&
                    !stop_pending && !chassis_claimed && !speed_target_pending;
    const uint32_t age = osKernelGetTickCount() - request.issued_ms;
    accepted = accepted && (session_claimed ||
               (age >= request.duration_ms && published.remaining_ms == 0U &&
                published.motor.phase_left == MOTOR_OFF &&
                published.motor.phase_right == MOTOR_OFF));
    if (accepted)
    {
        pending_targets[0] = left_rpm;
        pending_targets[1] = right_rpm;
        speed_target_pending = true;
        if (left_rpm != 0.0f || right_rpm != 0.0f) { session_claimed = true; }
    }
    taskEXIT_CRITICAL();
    return accepted;
}

bool wheel_io_chassis_set(float left_rpm, float right_rpm)
{
    taskENTER_CRITICAL();
    bool accepted = published.ready && !published.fault && !motor_guard_fault() && !stop_pending;
    if (!chassis_claimed)
    {
        const uint32_t age = osKernelGetTickCount() - request.issued_ms;
        accepted = accepted && !session_claimed && !speed_target_pending &&
                   age >= request.duration_ms && published.remaining_ms == 0U &&
                   published.motor.phase_left == MOTOR_OFF &&
                   published.motor.phase_right == MOTOR_OFF;
    }
    if (accepted)
    {
        chassis_claimed = true;
        chassis_targets[0] = left_rpm;
        chassis_targets[1] = right_rpm;
    }
    taskEXIT_CRITICAL();
    return accepted;
}

bool wheel_io_chassis_claimed(void)
{
    taskENTER_CRITICAL();
    const bool claimed = chassis_claimed;
    taskEXIT_CRITICAL();
    return claimed;
}

static void clear_motion_state(wheel_io_status_t *next, bool clear_request)
{
    targets[0] = targets[1] = 0.0f;
    wheel_speed_reset(&controllers[0]);
    wheel_speed_reset(&controllers[1]);
    next->speed_running = false;
    next->remaining_ms = 0U;
    next->requested_left = next->requested_right = 0;
    next->session = false;
    next->selected = 0U;
    taskENTER_CRITICAL();
    session_claimed = false;
    chassis_claimed = false;
    chassis_targets[0] = chassis_targets[1] = 0.0f;
    if (clear_request) { request = (drive_request_t){0}; }
    speed_target_pending = false;
    taskEXIT_CRITICAL();
}

void wheel_io_step(uint32_t now_ms, bool delay_error)
{
    const uint32_t sample_us = time_us_now();
    const uint32_t dt_us = sample_us - previous_us;
    previous_us = sample_us;
    wheel_io_status_t next = published;
    next.sample_ms = now_ms;
    next.interval_ms = now_ms - previous_ms;
    previous_ms = now_ms;
    next.dt_us = dt_us;
    encoder_sample(&next.encoder);
    speed_counts[0] += next.encoder.delta_left;
    speed_counts[1] += next.encoder.delta_right;
    if (++speed_sample_count == WHEEL_SPEED_SAMPLE_PERIOD_MS / WHEEL_SAMPLE_PERIOD_MS)
    {
        const uint32_t speed_dt_us = sample_us - speed_sample_us;
        const float alpha = (float)speed_dt_us / (WHEEL_SPEED_FILTER_TAU_US + (float)speed_dt_us);
        for (unsigned i = 0; i < 2U; i++)
        {
            const float raw_rpm = wheel_speed_measure_rpm(speed_counts[i], speed_dt_us);
            measured_rpm[i] += alpha * (raw_rpm - measured_rpm[i]);
        }
        speed_counts[0] = speed_counts[1] = 0;
        speed_sample_us = sample_us;
        speed_sample_count = 0U;
    }

    taskENTER_CRITICAL();
    bool stopped = stop_pending;
    stop_pending = false;
    const drive_request_t command = request;
    const bool has_target = !stopped && speed_target_pending;
    float new_targets[2] = {0.0f, 0.0f};
    if (has_target)
    {
        speed_target_pending = false;
        new_targets[0] = pending_targets[0];
        new_targets[1] = pending_targets[1];
    }
    taskEXIT_CRITICAL();

    if (delay_error || next.interval_ms > 2U * WHEEL_SAMPLE_PERIOD_MS || dt_us < 1000U ||
        dt_us > 20000U || motor_guard_fault())
    {
        next.fault = true;
        next.stop_reason = motor_guard_fault() ? 2U : 1U;
    }
    if (stopped || next.fault)
    {
        clear_motion_state(&next, next.fault);
        if (!next.fault) { next.stop_reason = 4U; }
    }
    else if (has_target)
    {
        /* 目标归零时保持 session，由速度环按斜坡减速后再释放占用。 */
        targets[0] = new_targets[0];
        targets[1] = new_targets[1];
        if (targets[0] != 0.0f || targets[1] != 0.0f)
        {
            next.session = true;
            next.speed_running = true;
            next.stop_reason = 0U;
            if (targets[0] != 0.0f) { next.selected |= 1U; }
            if (targets[1] != 0.0f) { next.selected |= 2U; }
        }
    }

    if (!stopped && !next.fault && chassis_claimed)
    {
        targets[0] = chassis_targets[0];
        targets[1] = chassis_targets[1];
        next.selected = 3U;
        next.session = true;
        next.speed_running = true;
        next.stop_reason = 0U;
    }

    bool brakes[2] = {false, false};
    if (!stopped && !next.fault && next.session && next.speed_running)
    {
        const int32_t delta[2] = {next.encoder.delta_left, next.encoder.delta_right};
        for (unsigned i = 0; i < 2U; i++)
        {
            if (next.selected & (1U << i))
            {
                wheel_speed_step(&controllers[i], targets[i], measured_rpm[i], delta[i], dt_us);
            }
            brakes[i] = controllers[i].phase != WHEEL_SPEED_OFF;
        }
        next.requested_left = controllers[0].requested;
        next.requested_right = controllers[1].requested;
        if (!chassis_claimed && targets[0] == 0.0f && targets[1] == 0.0f &&
            controllers[0].phase == WHEEL_SPEED_OFF && controllers[1].phase == WHEEL_SPEED_OFF)
        {
            next.speed_running = false;
            next.session = false;
            next.selected = 0U;
            taskENTER_CRITICAL();
            session_claimed = false;
            taskEXIT_CRITICAL();
        }
    }
    else if (!stopped && !next.fault && !next.session && !chassis_claimed)
    {
        const uint32_t age = now_ms - command.issued_ms;
        const bool active = age < command.duration_ms;
        next.remaining_ms = active ? command.duration_ms - age : 0U;
        next.requested_left = active ? command.left : 0;
        next.requested_right = active ? command.right : 0;
    }
    else
    {
        next.requested_left = next.requested_right = 0;
        next.remaining_ms = 0U;
    }

    taskENTER_CRITICAL();
    if (next.requested_left || next.requested_right || brakes[0] || brakes[1])
    {
        motor_guard_begin(HAL_GetTick());
    }
    const bool ok = next.session && next.speed_running
                        ? motor_update_with_brake(next.requested_left, next.requested_right,
                                                  brakes[0], brakes[1], now_ms)
                        : motor_update(next.requested_left, next.requested_right, now_ms);
    motor_get_status(&next.motor);
    taskEXIT_CRITICAL();

    if (!ok)
    {
        next.fault = true;
        next.stop_reason = 2U;
        clear_motion_state(&next, true);
    }
    else if (next.speed_running && !next.fault)
    {
        wheel_speed_feedback_output(&controllers[0], next.motor.applied_left, dt_us);
        wheel_speed_feedback_output(&controllers[1], next.motor.applied_right, dt_us);
    }

    for (unsigned i = 0; i < 2U; i++)
    {
        if (!next.speed_running || !(next.selected & (1U << i)))
        {
            controllers[i].feedback_rpm = measured_rpm[i];
        }
    }
    next.speed[0] = controllers[0];
    next.speed[1] = controllers[1];
    next.target_rpm[0] = targets[0];
    next.target_rpm[1] = targets[1];
    taskENTER_CRITICAL();
    published = next;
    taskEXIT_CRITICAL();
}

void wheel_io_get_status(wheel_io_status_t *status)
{
    taskENTER_CRITICAL();
    *status = published;
    status->session_claimed = session_claimed;
    status->stop_pending = stop_pending;
    if (motor_guard_fault())
    {
        status->fault = true;
        status->stop_reason = 2U;
        status->speed_running = false;
        status->requested_left = status->requested_right = 0;
        status->remaining_ms = 0U;
        status->motor = (motor_status_t){0};
    }
    taskEXIT_CRITICAL();
}
