#include "wheel_io.h"

#include "bsp/time_us.h"
#include "config/wheel_config.h"
#include "main.h"

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "task.h"

#include <math.h>
#include <string.h>

#define SPEED_QUEUE_CAPACITY 8U

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
static bool stop_pending, session_claimed, event_inflight;
static uint32_t next_id, previous_ms, previous_us;
static uint32_t speed_sample_us, speed_sample_count;
static int32_t speed_counts[2];
static float measured_rpm[2];
static speed_event_t events[SPEED_QUEUE_CAPACITY];
static speed_result_t results[SPEED_QUEUE_CAPACITY];
static uint8_t event_head, event_count, result_head, result_count, outstanding;
static wheel_speed_config_t staged[2];
static wheel_speed_t controllers[2];
static float targets[2];
static float chassis_targets[2];
static bool chassis_claimed;
static wheel_io_status_t published;

static bool queue_event_locked(speed_event_t *event)
{
    if (outstanding >= SPEED_QUEUE_CAPACITY) { return false; }
    event->id = ++next_id;
    if (event->id == 0U) { event->id = ++next_id; }
    events[(event_head + event_count) % SPEED_QUEUE_CAPACITY] = *event;
    event_count++;
    outstanding++;
    return true;
}

static void finish_event_locked(const speed_event_t *event, bool accepted, uint32_t now_ms)
{
    speed_result_t *result = &results[(result_head + result_count) % SPEED_QUEUE_CAPACITY];
    *result = (speed_result_t){.id = event->id, .applied_ms = now_ms, .type = event->type,
                               .accepted = accepted, .wheels = event->wheels};
    result->config[0] = controllers[0].config;
    result->config[1] = controllers[1].config;
    result_count++;
}

static void cancel_events_locked(uint32_t now_ms)
{
    while (event_count != 0U)
    {
        const speed_event_t event = events[event_head];
        event_head = (uint8_t)((event_head + 1U) % SPEED_QUEUE_CAPACITY);
        event_count--;
        finish_event_locked(&event, false, now_ms);
    }
}

bool wheel_io_init(uint32_t now_ms)
{
    request = (drive_request_t){0};
    published = (wheel_io_status_t){0};
    stop_pending = session_claimed = event_inflight = false;
    chassis_claimed = false;
    chassis_targets[0] = chassis_targets[1] = 0.0f;
    event_head = event_count = result_head = result_count = outstanding = 0U;
    next_id = 0U;
    targets[0] = targets[1] = 0.0f;
    for (unsigned i = 0; i < 2U; i++)
    {
        wheel_speed_init(&controllers[i]);
        staged[i] = controllers[i].config;
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
    cancel_events_locked(osKernelGetTickCount());
    taskEXIT_CRITICAL();
}

bool wheel_io_speed_stage(unsigned side, speed_param_t parameter, float value, bool live,
                          uint32_t *id)
{
    if (side > 1U || !isfinite(value)) { return false; }
    if ((parameter == SPEED_PARAM_KP && (value < 0.0f || value > 1000.0f)) ||
        (parameter == SPEED_PARAM_KI && (value < 0.0f || value > 10000.0f)) ||
        (parameter == SPEED_PARAM_RAMP && (value < 0.0f || value > 10000.0f)) ||
        (parameter == SPEED_PARAM_LIMIT && (value < 1.0f || value > 800.0f || truncf(value) != value)) ||
        parameter > SPEED_PARAM_RAMP) { return false; }
    taskENTER_CRITICAL();
    wheel_speed_config_t next = staged[side];
    if (parameter == SPEED_PARAM_KP) { next.kp = value; }
    else if (parameter == SPEED_PARAM_KI) { next.ki = value; }
    else if (parameter == SPEED_PARAM_RAMP) { next.ramp_rpm_s = value; }
    else { next.limit = (int16_t)value; }
    bool accepted = !published.fault && !motor_guard_fault() && !stop_pending &&
                    (!live || !chassis_claimed);
    if (accepted && live)
    {
        speed_event_t event = {.type = SPEED_APPLY, .wheels = (uint8_t)(1U << side)};
        event.config[side] = next;
        accepted = wheel_speed_config_valid(&next) && queue_event_locked(&event);
        if (accepted) { *id = event.id; }
    }
    if (accepted)
    {
        staged[side] = next;
        if (!live) { *id = 0U; }
    }
    taskEXIT_CRITICAL();
    return accepted;
}

void wheel_io_speed_get_staged(wheel_speed_config_t config[2])
{
    taskENTER_CRITICAL();
    config[0] = staged[0];
    config[1] = staged[1];
    taskEXIT_CRITICAL();
}

bool wheel_io_speed_enqueue(speed_event_t *event)
{
    if (event->wheels == 0U || event->wheels > 3U) { return false; }
    taskENTER_CRITICAL();
    bool accepted = published.ready && !published.fault && !motor_guard_fault() &&
                    !stop_pending && !chassis_claimed;
    if (event->type == SPEED_TARGET)
    {
        const uint32_t age = osKernelGetTickCount() - request.issued_ms;
        accepted = accepted && (session_claimed ||
                   (age >= request.duration_ms && published.remaining_ms == 0U &&
                    published.motor.phase_left == MOTOR_OFF && published.motor.phase_right == MOTOR_OFF));
        for (unsigned i = 0; i < 2U; i++)
        {
            if ((event->wheels & (1U << i)) &&
                (!isfinite(event->target[i]) || event->target[i] < WHEEL_TARGET_MIN_RPM ||
                 event->target[i] > WHEEL_TARGET_MAX_RPM)) { accepted = false; }
        }
    }
    else if (event->type == SPEED_APPLY)
    {
        for (unsigned i = 0; i < 2U; i++)
        {
            event->config[i] = staged[i];
            if ((event->wheels & (1U << i)) && !wheel_speed_config_valid(&event->config[i]))
            {
                accepted = false;
            }
        }
    }
    else { accepted = false; }
    if (accepted) { accepted = queue_event_locked(event); }
    if (accepted && event->type == SPEED_TARGET &&
        (((event->wheels & 1U) && event->target[0] != 0.0f) ||
         ((event->wheels & 2U) && event->target[1] != 0.0f)))
    {
        session_claimed = true;
    }
    taskEXIT_CRITICAL();
    return accepted;
}

bool wheel_io_speed_result(speed_result_t *result)
{
    taskENTER_CRITICAL();
    const bool available = result_count != 0U;
    if (available)
    {
        *result = results[result_head];
        result_head = (uint8_t)((result_head + 1U) % SPEED_QUEUE_CAPACITY);
        result_count--;
        outstanding--;
    }
    taskEXIT_CRITICAL();
    return available;
}

bool wheel_io_speed_result_pending(void)
{
    taskENTER_CRITICAL();
    const bool pending = result_count != 0U;
    taskEXIT_CRITICAL();
    return pending;
}


static bool apply_event(const speed_event_t *event, wheel_io_status_t *next, uint32_t now_ms)
{
    if (event->type == SPEED_APPLY)
    {
        for (unsigned i = 0; i < 2U; i++)
        {
            if ((event->wheels & (1U << i)) &&
                !wheel_speed_apply_config(&controllers[i], &event->config[i])) { return false; }
        }
        next->applied_id = event->id;
        next->applied_ms = now_ms;
        return true;
    }
    if (event->type == SPEED_TARGET)
    {
        for (unsigned i = 0; i < 2U; i++)
        {
            if ((event->wheels & (1U << i)) && event->target[i] != 0.0f &&
                !controllers[i].config.configured) { return false; }
        }
        for (unsigned i = 0; i < 2U; i++)
        {
            if (event->wheels & (1U << i))
            {
                targets[i] = event->target[i];
                if (targets[i] != 0.0f) { next->selected |= (uint8_t)(1U << i); }
            }
        }
        if (targets[0] != 0.0f || targets[1] != 0.0f)
        {
            next->session = true;
            next->speed_running = true;
            next->stop_reason = 0U;
        }
        return true;
    }
    return false;
}

bool wheel_io_chassis_set(float left_rpm, float right_rpm)
{
    if (!isfinite(left_rpm) || !isfinite(right_rpm)) { return false; }
    taskENTER_CRITICAL();
    bool accepted = published.ready && !published.fault && !motor_guard_fault() && !stop_pending;
    if (!chassis_claimed)
    {
        const uint32_t age = osKernelGetTickCount() - request.issued_ms;
        accepted = accepted && !session_claimed && !event_inflight && event_count == 0U &&
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
    bool speed_sample_invalid = false;
    if (++speed_sample_count == WHEEL_SPEED_SAMPLE_PERIOD_MS / WHEEL_SAMPLE_PERIOD_MS)
    {
        const uint32_t speed_dt_us = sample_us - speed_sample_us;
        speed_sample_invalid = speed_dt_us == 0U;
        if (speed_dt_us != 0U)
        {
            const float alpha = (float)speed_dt_us / (WHEEL_SPEED_FILTER_TAU_US + (float)speed_dt_us);
            for (unsigned i = 0; i < 2U; i++)
            {
                const float raw_rpm = wheel_speed_measure_rpm(speed_counts[i], speed_dt_us);
                measured_rpm[i] += alpha * (raw_rpm - measured_rpm[i]);
            }
        }
        speed_counts[0] = speed_counts[1] = 0;
        speed_sample_us = sample_us;
        speed_sample_count = 0U;
    }

    taskENTER_CRITICAL();
    bool stopped = stop_pending;
    stop_pending = false;
    const drive_request_t command = request;
    speed_event_t event = {0};
    const bool has_event = !stopped && event_count != 0U;
    if (has_event)
    {
        event_inflight = true;
        event = events[event_head];
        event_head = (uint8_t)((event_head + 1U) % SPEED_QUEUE_CAPACITY);
        event_count--;
    }
    taskEXIT_CRITICAL();

    if (delay_error || speed_sample_invalid || next.interval_ms > 2U * WHEEL_SAMPLE_PERIOD_MS || dt_us < 1000U ||
        dt_us > 20000U || motor_guard_fault())
    {
        next.fault = true;
        next.stop_reason = motor_guard_fault() ? 2U : 1U;
    }
    if (stopped || next.fault)
    {
        if (has_event)
        {
            taskENTER_CRITICAL();
            finish_event_locked(&event, false, now_ms);
            event_inflight = false;
            taskEXIT_CRITICAL();
        }
        targets[0] = targets[1] = 0.0f;
        wheel_speed_reset(&controllers[0]);
        wheel_speed_reset(&controllers[1]);
        next.speed_running = false;
        next.remaining_ms = 0U;
        next.requested_left = next.requested_right = 0;
        if (!next.fault) { next.stop_reason = 4U; }
        next.session = false;
        next.selected = 0U;
        taskENTER_CRITICAL();
        session_claimed = false;
        chassis_claimed = false;
        chassis_targets[0] = chassis_targets[1] = 0.0f;
        cancel_events_locked(now_ms);
        taskEXIT_CRITICAL();
    }
    else if (has_event)
    {
        const bool accepted = apply_event(&event, &next, now_ms);
        taskENTER_CRITICAL();
        if (!next.session && event_count == 0U)
        {
            session_claimed = false;
        }
        finish_event_locked(&event, accepted, now_ms);
        event_inflight = false;
        taskEXIT_CRITICAL();
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
            if ((next.selected & (1U << i)) &&
                !wheel_speed_step(&controllers[i], targets[i], measured_rpm[i], delta[i], dt_us))
            {
                next.fault = true;
                next.stop_reason = 3U;
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
            if (event_count == 0U) { session_claimed = false; }
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
    if (next.fault)
    {
        next.requested_left = next.requested_right = 0;
        brakes[0] = brakes[1] = false;
    }
    if (next.requested_left || next.requested_right || brakes[0] || brakes[1])
    {
        motor_guard_begin(HAL_GetTick());
    }
    const bool ok = next.session && next.speed_running
                        ? motor_update_with_brake(next.requested_left, next.requested_right,
                                                  brakes[0], brakes[1], now_ms)
                        : motor_update(next.requested_left, next.requested_right, now_ms);
    if (!ok)
    {
        next.fault = true;
        next.stop_reason = 2U;
        next.requested_left = next.requested_right = 0;
        next.remaining_ms = 0U;
    }
    motor_get_status(&next.motor);
    taskEXIT_CRITICAL();

    if (next.speed_running && !next.fault)
    {
        if (!wheel_speed_feedback_output(&controllers[0], next.motor.applied_left, dt_us) ||
            !wheel_speed_feedback_output(&controllers[1], next.motor.applied_right, dt_us))
        {
            next.fault = true;
            next.stop_reason = 3U;
            taskENTER_CRITICAL();
            motor_force_outputs_low();
            motor_stop(now_ms);
            motor_get_status(&next.motor);
            taskEXIT_CRITICAL();
            next.requested_left = next.requested_right = 0;
            next.remaining_ms = 0U;
        }
    }
    if (next.fault)
    {
        next.session = next.speed_running = false;
        targets[0] = targets[1] = 0.0f;
        wheel_speed_reset(&controllers[0]);
        wheel_speed_reset(&controllers[1]);
        taskENTER_CRITICAL();
        session_claimed = false;
        chassis_claimed = false;
        chassis_targets[0] = chassis_targets[1] = 0.0f;
        request = (drive_request_t){0};
        cancel_events_locked(now_ms);
        taskEXIT_CRITICAL();
    }
    if (dt_us >= 1000U && dt_us <= 20000U)
    {
        for (unsigned i = 0; i < 2U; i++)
        {
            if (!next.speed_running || !(next.selected & (1U << i)))
            {
                controllers[i].feedback_rpm = measured_rpm[i];
            }
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
