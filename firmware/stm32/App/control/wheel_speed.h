#ifndef ROBOT_WHEEL_SPEED_H
#define ROBOT_WHEEL_SPEED_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float kp;
    float ki;
    float ramp_rpm_s;
    int16_t limit;
} wheel_speed_config_t;

typedef enum
{
    WHEEL_SPEED_OFF,
    WHEEL_SPEED_TRACK,
    WHEEL_SPEED_BRAKE
} wheel_speed_phase_t;

typedef struct
{
    wheel_speed_config_t config;
    wheel_speed_phase_t phase;
    float target_rpm;
    float effective_rpm;
    float feedback_rpm;
    float error_rpm;
    float proportional;
    float integral;
    float unconstrained;
    int16_t requested;
    int16_t applied;
    bool saturated;
    int8_t direction;
    uint32_t quiet_us;
    uint8_t quiet_counts;
} wheel_speed_t;

void wheel_speed_init(wheel_speed_t *wheel);
/* dt_us 为有效的非零采样间隔。 */
float wheel_speed_measure_rpm(int32_t delta_counts, uint32_t dt_us);
void wheel_speed_reset(wheel_speed_t *wheel);
bool wheel_speed_step(wheel_speed_t *wheel, float target_rpm, float feedback_rpm,
                      int32_t delta_counts, uint32_t dt_us);
bool wheel_speed_feedback_output(wheel_speed_t *wheel, int16_t applied, uint32_t dt_us);

#endif
