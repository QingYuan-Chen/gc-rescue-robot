#include "wheel_speed.h"
#include "config/wheel_config.h"

#include <math.h>

float wheel_speed_measure_rpm(int32_t delta_counts, uint32_t dt_us)
{
    return (float)delta_counts * 60000000.0f /
           (WHEEL_ENCODER_COUNTS_PER_REV * (float)dt_us);
}

static float clamp(float value, float low, float high)
{
    if (value < low) { return low; }
    if (value > high) { return high; }
    return value;
}

void wheel_speed_init(wheel_speed_t *wheel)
{
    *wheel = (wheel_speed_t){.config = {.kp = 33.0f, .ki = 40.0f,
                                        .limit = WHEEL_COMMAND_MAX_DUTY,
                                        .ramp_rpm_s = 300.0f, .configured = true}};
}

bool wheel_speed_config_valid(const wheel_speed_config_t *config)
{
    return isfinite(config->kp) && isfinite(config->ki) && isfinite(config->ramp_rpm_s) &&
           config->kp >= 0.0f && config->kp <= 1000.0f && config->ki >= 0.0f &&
           config->ki <= 10000.0f && config->ramp_rpm_s >= 0.0f &&
           config->ramp_rpm_s <= 10000.0f && config->limit >= 1 &&
           config->limit <= WHEEL_COMMAND_MAX_DUTY;
}

bool wheel_speed_apply_config(wheel_speed_t *wheel, const wheel_speed_config_t *config)
{
    if (!wheel_speed_config_valid(config)) { return false; }
    wheel->config = *config;
    wheel->config.configured = true;
    if (config->ki == 0.0f || wheel->phase != WHEEL_SPEED_TRACK)
    {
        wheel->integral = 0.0f;
    }
    else
    {
        wheel->integral = clamp((float)wheel->applied, -(float)config->limit,
                                (float)config->limit) - config->kp * wheel->error_rpm;
    }
    return true;
}

void wheel_speed_reset(wheel_speed_t *wheel)
{
    const wheel_speed_config_t config = wheel->config;
    *wheel = (wheel_speed_t){.config = config};
}

bool wheel_speed_step(wheel_speed_t *wheel, float target_rpm, float feedback_rpm,
                      int32_t delta_counts, uint32_t dt_us)
{
    if (!wheel->config.configured || !isfinite(target_rpm) || !isfinite(feedback_rpm) ||
        target_rpm < WHEEL_TARGET_MIN_RPM || target_rpm > WHEEL_TARGET_MAX_RPM ||
        dt_us < 1000U || dt_us > 20000U) { return false; }
    const float dt = (float)dt_us * 0.000001f;
    wheel->feedback_rpm = feedback_rpm;
    wheel->target_rpm = target_rpm;
    const int8_t target_direction = target_rpm > 0.0f ? 1 : target_rpm < 0.0f ? -1 : 0;
    if (wheel->phase == WHEEL_SPEED_OFF && target_direction != 0)
    {
        wheel->phase = WHEEL_SPEED_TRACK;
        wheel->direction = target_direction;
    }
    if (wheel->phase == WHEEL_SPEED_BRAKE)
    {
        const uint32_t magnitude = delta_counts < 0 ? (uint32_t)(-(int64_t)delta_counts)
                                                    : (uint32_t)delta_counts;
        if (magnitude > 1U || wheel->quiet_counts + magnitude > 1U)
        {
            wheel->quiet_us = 0U;
            wheel->quiet_counts = 0U;
        }
        else
        {
            wheel->quiet_counts += (uint8_t)magnitude;
            wheel->quiet_us += dt_us;
        }
        if (wheel->quiet_us >= 100000U)
        {
            wheel_speed_reset(wheel);
        }
        return true;
    }
    if (wheel->phase == WHEEL_SPEED_OFF) { return true; }

    const float desired = target_direction != 0 && target_direction != wheel->direction
                              ? 0.0f : target_rpm;
    const float max_delta = wheel->config.ramp_rpm_s * dt;
    if (wheel->config.ramp_rpm_s == 0.0f) { wheel->effective_rpm = desired; }
    else
    {
        wheel->effective_rpm += clamp(desired - wheel->effective_rpm, -max_delta, max_delta);
    }
    if (desired == 0.0f && wheel->effective_rpm == 0.0f)
    {
        wheel->phase = WHEEL_SPEED_BRAKE;
        wheel->integral = 0.0f;
        wheel->requested = 0;
        wheel->proportional = 0.0f;
        wheel->unconstrained = 0.0f;
        return true;
    }
    wheel->error_rpm = wheel->effective_rpm - wheel->feedback_rpm;
    wheel->proportional = wheel->config.kp * wheel->error_rpm;
    wheel->unconstrained = wheel->proportional + wheel->integral;
    if (!isfinite(wheel->unconstrained)) { return false; }
    const float limited = clamp(wheel->unconstrained, -(float)wheel->config.limit,
                                (float)wheel->config.limit);
    wheel->saturated = limited != wheel->unconstrained;
    wheel->requested = (int16_t)(limited >= 0.0f ? limited + 0.5f : limited - 0.5f);
    return true;
}

bool wheel_speed_feedback_output(wheel_speed_t *wheel, int16_t applied, uint32_t dt_us)
{
    wheel->applied = applied;
    if (wheel->phase != WHEEL_SPEED_TRACK || wheel->config.ki == 0.0f)
    {
        wheel->integral = 0.0f;
        return true;
    }
    const float dt = (float)dt_us * 0.000001f;
    wheel->integral += (wheel->config.ki * wheel->error_rpm +
                        10.0f * ((float)applied - wheel->unconstrained)) * dt;
    return isfinite(wheel->integral) && fabsf(wheel->integral) <= 1000000000.0f;
}
