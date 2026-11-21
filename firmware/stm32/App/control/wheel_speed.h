#ifndef ROBOT_WHEEL_SPEED_H
#define ROBOT_WHEEL_SPEED_H

#include <stdbool.h>
#include <stdint.h>

/* 速度环固定整定参数（实车已验收）：Kp、Ki 与目标斜坡。 */
#define WHEEL_SPEED_KP 33.0f
#define WHEEL_SPEED_KI 40.0f
#define WHEEL_SPEED_RAMP_RPM_S 300.0f

typedef enum
{
    WHEEL_SPEED_OFF,
    WHEEL_SPEED_TRACK,
    WHEEL_SPEED_BRAKE
} wheel_speed_phase_t;

typedef struct
{
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

/* dt_us 为有效的非零采样间隔。 */
float wheel_speed_measure_rpm(int32_t delta_counts, uint32_t dt_us);
void wheel_speed_reset(wheel_speed_t *wheel);
void wheel_speed_step(wheel_speed_t *wheel, float target_rpm, float feedback_rpm,
                      int32_t delta_counts, uint32_t dt_us);
void wheel_speed_feedback_output(wheel_speed_t *wheel, int16_t applied, uint32_t dt_us);

#endif
