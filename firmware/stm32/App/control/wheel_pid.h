#ifndef ROBOT_WHEEL_PID_H
#define ROBOT_WHEEL_PID_H

#include "pid.h"

#include <stdint.h>

/* 调用方分配实例，只通过下面的公开接口更新和读取控制器。 */
typedef struct
{
    pid_pi_params_t params;
    pid_pi_state_t state;
} wheel_pid_t;

typedef struct
{
    float error_counts;
    float proportional_permille;
    float integral_permille;
    float output_permille;
    float next_integral_permille;
} wheel_pid_status_t;

void wheel_pid_init(wheel_pid_t *controller, const pid_pi_params_t *params);
void wheel_pid_reset(wheel_pid_t *controller);

/* 输入均为 counts/10 ms，输出直接为占空比千分比，不再经过原 CCR 尺度。 */
float wheel_pid_prepare(wheel_pid_t *controller, float target_delta_counts,
                        int32_t measured_delta_counts);
void wheel_pid_track(wheel_pid_t *controller, int32_t applied_duty_permille, bool constrained);
void wheel_pid_get_status(const wheel_pid_t *controller, wheel_pid_status_t *status);

#endif
