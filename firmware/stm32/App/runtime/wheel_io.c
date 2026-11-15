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
_Static_assert(WHEEL_POSITION_PERIOD_MS % WHEEL_SAMPLE_PERIOD_MS == 0U,
               "Position period must be a multiple of the control period");

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
static wheel_io_status_t published;
static bool position_claimed, position_pending, position_started;
static bool position_slider, position_retarget_pending;
static int32_t position_retarget;
static wheel_position_status_t position, position_published;
static wheel_position_config_t position_config, position_active_config;
static int64_t position_start[2];
static int64_t position_previous_actual[2];
static float position_integral[2];
/* 0 未参与，1 运动，2 停稳中，3 已停稳。 */
static uint8_t position_phase[2];
static int8_t position_direction[2];
static uint32_t position_last_update;

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
    position = position_published = (wheel_position_status_t){0};
    position_config = (wheel_position_config_t){.kp = WHEEL_POSITION_KP,
        .ki = WHEEL_POSITION_KI, .kd = WHEEL_POSITION_KD,
        .max_rpm = WHEEL_POSITION_MAX_RPM, .tolerance_counts = WHEEL_POSITION_TOLERANCE_COUNTS};
    position_claimed = position_pending = position_started = false;
    position_slider = position_retarget_pending = false;
    stop_pending = session_claimed = event_inflight = false;
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
                          !stop_pending && !session_claimed && !position_claimed;
    if (accepted) { request = (drive_request_t){left, right, osKernelGetTickCount(), duration_ms}; }
    taskEXIT_CRITICAL();
    return accepted;
}

void wheel_io_stop(void)
{
    taskENTER_CRITICAL();
    request = (drive_request_t){0};
    stop_pending = true;
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
                    (!live || !position_claimed);
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
                    !stop_pending && !position_claimed;
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

static bool position_move_locked(int32_t left, int32_t right, uint32_t *id)
{
    const uint32_t age = osKernelGetTickCount() - request.issued_ms;
    const bool accepted = published.ready && !published.fault && !motor_guard_fault() &&
                          !stop_pending && !session_claimed && !position_claimed &&
                          !published.session && event_count == 0U && !event_inflight &&
                          age >= request.duration_ms && published.remaining_ms == 0U &&
                          published.motor.phase_left == MOTOR_OFF &&
                          published.motor.phase_right == MOTOR_OFF;
    if (accepted)
    {
        if (++next_id == 0U) { next_id++; }
        position_published = (wheel_position_status_t){.id = next_id, .state = POSITION_QUEUED,
                                                       .target = {left, right},
                                                       .error = {left, right}};
        *id = next_id;
        position_claimed = position_pending = true;
        position_active_config = position_config;
    }
    return accepted;
}

bool wheel_io_position_move(int32_t left, int32_t right, uint32_t *id)
{
    if (left < -WHEEL_POSITION_MAX_COUNTS || left > WHEEL_POSITION_MAX_COUNTS ||
        right < -WHEEL_POSITION_MAX_COUNTS || right > WHEEL_POSITION_MAX_COUNTS ||
        (left == 0 && right == 0)) { return false; }
    taskENTER_CRITICAL();
    const bool accepted = position_move_locked(left, right, id);
    if (accepted) { position_slider = position_retarget_pending = false; }
    taskEXIT_CRITICAL();
    return accepted;
}

bool wheel_io_position_slider_left(int32_t target, uint32_t *id)
{
    if (target < -WHEEL_POSITION_MAX_COUNTS || target > WHEEL_POSITION_MAX_COUNTS) { return false; }
    taskENTER_CRITICAL();
    bool accepted = published.ready && !published.fault && !motor_guard_fault() && !stop_pending;
    if (accepted && position_claimed && position_slider &&
        (position_pending || position_published.state == POSITION_RUNNING))
    {
        position_retarget = target;
        position_retarget_pending = true;
        *id = position_published.id;
    }
    else if (accepted && target != 0)
    {
        accepted = position_move_locked(target, 0, id);
        if (accepted) { position_slider = true; position_retarget_pending = false; }
    }
    else if (accepted && !position_claimed && !session_claimed && !published.session &&
             event_count == 0U && !event_inflight && published.remaining_ms == 0U &&
             published.motor.phase_left == MOTOR_OFF && published.motor.phase_right == MOTOR_OFF &&
             osKernelGetTickCount() - request.issued_ms >= request.duration_ms)
    {
        *id = 0U;
    }
    else { accepted = false; }
    taskEXIT_CRITICAL();
    return accepted;
}

void wheel_io_position_get(wheel_position_status_t *status)
{
    taskENTER_CRITICAL();
    *status = position_published;
    taskEXIT_CRITICAL();
}

void wheel_io_position_snapshot(wheel_position_status_t *position_status,
                                wheel_io_status_t *wheel_status)
{
    taskENTER_CRITICAL();
    *position_status = position_published;
    *wheel_status = published;
    taskEXIT_CRITICAL();
}

void wheel_io_position_get_config(wheel_position_config_t *config)
{
    taskENTER_CRITICAL();
    *config = position_config;
    taskEXIT_CRITICAL();
}

bool wheel_io_position_set_config(const wheel_position_config_t *config)
{
    if (!isfinite(config->kp) || !isfinite(config->ki) || !isfinite(config->kd) ||
        !isfinite(config->max_rpm) || config->kp < 0.001f ||
        config->ki < 0.0f || config->ki > 2.0f || config->kd < 0.0f || config->kd > 2.0f ||
        config->kp > 2.0f || config->max_rpm < 1.0f ||
        config->max_rpm > WHEEL_POSITION_MAX_RPM || config->tolerance_counts < 1 ||
        config->tolerance_counts > 100) { return false; }
    taskENTER_CRITICAL();
    const bool accepted = published.ready && !published.fault && !motor_guard_fault() &&
                          !stop_pending && !position_claimed && !session_claimed &&
                          !published.session && event_count == 0U && !event_inflight &&
                          published.motor.phase_left == MOTOR_OFF &&
                          published.motor.phase_right == MOTOR_OFF &&
                          osKernelGetTickCount() - request.issued_ms >= request.duration_ms;
    if (accepted) { position_config = *config; }
    taskEXIT_CRITICAL();
    return accepted;
}

static void position_update(const encoder_sample_t *encoder, const motor_status_t *motor,
                            uint32_t now_ms)
{
    const int64_t totals[2] = {encoder->total_left, encoder->total_right};
    const motor_phase_t motor_phase[2] = {motor->phase_left, motor->phase_right};
    const uint32_t elapsed_ms = now_ms - position_last_update;
    const bool update = elapsed_ms >= WHEEL_POSITION_PERIOD_MS;
    if (update) { position_last_update = now_ms; }
    position.elapsed_ms = now_ms - position.started_ms;
    for (unsigned i = 0; i < 2U; i++)
    {
        position.actual[i] = totals[i] - position_start[i];
        position.error[i] = (int64_t)position.target[i] - position.actual[i];
        if (position_phase[i] == 0U) { continue; }
        const int64_t error = position.error[i];
        const bool in_tolerance = error >= -position_active_config.tolerance_counts &&
                                  error <= position_active_config.tolerance_counts;
        if (position_phase[i] == 1U &&
            (in_tolerance || (position_direction[i] > 0 && error < 0) ||
             (position_direction[i] < 0 && error > 0)))
        {
            position_phase[i] = 2U;
            targets[i] = 0.0f;
            position_integral[i] = 0.0f;
        }
        if (position_phase[i] == 2U && controllers[i].phase == WHEEL_SPEED_OFF &&
            motor_phase[i] == MOTOR_OFF)
        {
            position_phase[i] = 3U;
        }
        if (position_phase[i] == 3U && !in_tolerance)
        {
            if (position.corrections[i] != 0U)
            {
                position.state = POSITION_OVERSHOOT;
                return;
            }
            position.corrections[i]++;
            position_phase[i] = 1U;
            position_direction[i] = error > 0 ? 1 : -1;
            position_previous_actual[i] = position.actual[i];
            position_integral[i] = 0.0f;
        }
        if (position_phase[i] == 1U && update)
        {
            const float dt = (float)elapsed_ms * 0.001f;
            const float p = (float)error * position_active_config.kp;
            const float d = -(float)(position.actual[i] - position_previous_actual[i]) *
                            position_active_config.kd / dt;
            float next_integral = position_integral[i] +
                                  position_active_config.ki * (float)error * dt;
            if (next_integral > position_active_config.max_rpm) { next_integral = position_active_config.max_rpm; }
            if (next_integral < -position_active_config.max_rpm) { next_integral = -position_active_config.max_rpm; }
            const float candidate = p + next_integral + d;
            if ((candidate <= position_active_config.max_rpm &&
                 candidate >= -position_active_config.max_rpm) ||
                (candidate > position_active_config.max_rpm && error < 0) ||
                (candidate < -position_active_config.max_rpm && error > 0))
            {
                position_integral[i] = next_integral;
            }
            position_previous_actual[i] = position.actual[i];
            float rpm = p + position_integral[i] + d;
            if (rpm > position_active_config.max_rpm) { rpm = position_active_config.max_rpm; }
            if (rpm < -position_active_config.max_rpm) { rpm = -position_active_config.max_rpm; }
            if ((position_direction[i] > 0 && rpm < 0.0f) ||
                (position_direction[i] < 0 && rpm > 0.0f)) { rpm = 0.0f; }
            targets[i] = rpm;
        }
        position.target_rpm[i] = targets[i];
    }
    if ((position_phase[0] == 0U || position_phase[0] == 3U) &&
        (position_phase[1] == 0U || position_phase[1] == 3U))
    {
        position.state = POSITION_DONE;
    }
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
    const bool start_position = position_pending && !stopped;
    if (position_pending)
    {
        position = position_published;
        position_started = false;
        position_pending = false;
        if (stopped) { position.state = POSITION_STOPPED; }
    }
    const bool retarget = position_retarget_pending && !stopped;
    const int32_t new_target = position_retarget;
    position_retarget_pending = false;
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
    if (start_position && !next.fault)
    {
        if (retarget) { position.target[0] = new_target; }
        position.state = POSITION_RUNNING;
        position_started = true;
        position.started_ms = now_ms;
        position_last_update = now_ms - WHEEL_POSITION_PERIOD_MS;
        position_start[0] = next.encoder.total_left;
        position_start[1] = next.encoder.total_right;
        next.selected = 0U;
        for (unsigned i = 0; i < 2U; i++)
        {
            position_phase[i] = position.target[i] == 0 ? 0U : 1U;
            position_direction[i] = position.target[i] > 0 ? 1 : -1;
            position_previous_actual[i] = 0;
            position_integral[i] = 0.0f;
            if (position_phase[i] != 0U) { next.selected |= (uint8_t)(1U << i); }
        }
        next.session = next.speed_running = true;
        next.stop_reason = 0U;
    }
    else if (retarget && !next.fault &&
             (position.state == POSITION_RUNNING || position.state == POSITION_DONE))
    {
        if (position.state == POSITION_DONE)
        {
            position.state = POSITION_RUNNING;
            position_phase[0] = 3U;
            next.selected = 1U;
            next.session = next.speed_running = true;
        }
        position.target[0] = new_target;
        position.corrections[0] = 0U;
    }
    if (position.state == POSITION_RUNNING)
    {
        if (stopped || next.fault)
        {
            position.state = next.fault ? POSITION_FAULT : POSITION_STOPPED;
            position.elapsed_ms = now_ms - position.started_ms;
        }
        else if (now_ms - position.started_ms >= WHEEL_POSITION_TIMEOUT_MS)
        {
            position.elapsed_ms = now_ms - position.started_ms;
            position.state = POSITION_TIMEOUT;
            stopped = true;
        }
        else
        {
            position_update(&next.encoder, &next.motor, now_ms);
            if (position.state != POSITION_RUNNING) { stopped = true; }
        }
    }
    else if (start_position && next.fault) { position.state = POSITION_FAULT; }
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
        if (position.state != POSITION_RUNNING && targets[0] == 0.0f && targets[1] == 0.0f &&
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
    else if (!stopped && !next.fault && !next.session)
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
        if (position.state == POSITION_RUNNING)
        {
            position.state = POSITION_FAULT;
            position.elapsed_ms = now_ms - position.started_ms;
        }
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
        if (position_claimed && (position.state == POSITION_RUNNING ||
                                 position.state == POSITION_DONE))
        {
            position.state = POSITION_FAULT;
            if (position_started) { position.elapsed_ms = now_ms - position.started_ms; }
        }
        next.session = next.speed_running = false;
        targets[0] = targets[1] = 0.0f;
        wheel_speed_reset(&controllers[0]);
        wheel_speed_reset(&controllers[1]);
        taskENTER_CRITICAL();
        session_claimed = false;
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
    if (position.state != POSITION_RUNNING)
    {
        if (position_started && position_claimed)
        {
            position.actual[0] = next.encoder.total_left - position_start[0];
            position.actual[1] = next.encoder.total_right - position_start[1];
            position.error[0] = (int64_t)position.target[0] - position.actual[0];
            position.error[1] = (int64_t)position.target[1] - position.actual[1];
        }
        position.target_rpm[0] = position.target_rpm[1] = 0.0f;
    }
    taskENTER_CRITICAL();
    published = next;
    if (position_claimed && !position_pending)
    {
        position_published = position;
        if (position.state != POSITION_RUNNING && position.state != POSITION_DONE)
        {
            position_retarget_pending = false;
        }
        if (position.state != POSITION_RUNNING && !position_retarget_pending && next.motor.phase_left == MOTOR_OFF &&
            next.motor.phase_right == MOTOR_OFF)
        {
            position_claimed = false;
        }
    }
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
