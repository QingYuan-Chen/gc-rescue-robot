#include "wheel_speed.h"
#include "config/wheel_config.h"

float wheel_speed_measure_rpm(int32_t delta_counts, uint32_t dt_us)
{
    return (float)delta_counts * 60000000.0f / (WHEEL_ENCODER_COUNTS_PER_REV * (float)dt_us);
}

static float clamp(float value, float low, float high)
{
    if (value < low) { return low; }
    if (value > high) { return high; }
    return value;
}

void wheel_speed_reset(wheel_speed_t *wheel)
{
    *wheel = (wheel_speed_t){0};
}

void wheel_speed_step(wheel_speed_t *wheel, float target_rpm, float feedback_rpm,
                      int32_t delta_counts, uint32_t dt_us)
{
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
        return;
    }
    if (wheel->phase == WHEEL_SPEED_OFF) { return; }

    const float desired = target_direction != 0 && target_direction != wheel->direction
                              ? 0.0f : target_rpm;
    const float max_delta = WHEEL_SPEED_RAMP_RPM_S * dt;
    wheel->effective_rpm += clamp(desired - wheel->effective_rpm, -max_delta, max_delta);
    if (desired == 0.0f && wheel->effective_rpm == 0.0f)
    {
        wheel->phase = WHEEL_SPEED_BRAKE;
        wheel->integral = 0.0f;
        wheel->requested = 0;
        wheel->proportional = 0.0f;
        wheel->unconstrained = 0.0f;
        return;
    }
    wheel->error_rpm = wheel->effective_rpm - wheel->feedback_rpm;
    wheel->proportional = WHEEL_SPEED_KP * wheel->error_rpm;
    wheel->unconstrained = wheel->proportional + wheel->integral;
    const float limited = clamp(wheel->unconstrained, -(float)WHEEL_COMMAND_MAX_DUTY,
                                (float)WHEEL_COMMAND_MAX_DUTY);
    wheel->saturated = limited != wheel->unconstrained;
    wheel->requested = (int16_t)(limited >= 0.0f ? limited + 0.5f : limited - 0.5f);
}

void wheel_speed_feedback_output(wheel_speed_t *wheel, int16_t applied, uint32_t dt_us)
{
    wheel->applied = applied;
    if (wheel->phase != WHEEL_SPEED_TRACK)
    {
        wheel->integral = 0.0f;
        return;
    }
    const float dt = (float)dt_us * 0.000001f;
    wheel->integral += (WHEEL_SPEED_KI * wheel->error_rpm +
                        10.0f * ((float)applied - wheel->unconstrained)) * dt;
}
