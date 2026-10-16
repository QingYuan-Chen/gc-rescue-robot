#include "wheel_pid.h"

#include "config/wheel_config.h"

_Static_assert(WHEEL_PERIOD_MS == 10U, "车轮目标与反馈单位要求固定 10 ms 控制周期");

void wheel_pid_init(wheel_pid_t *controller, const pid_pi_params_t *params)
{
    controller->params = *params;
    wheel_pid_reset(controller);
}

void wheel_pid_reset(wheel_pid_t *controller)
{
    pid_pi_reset(&controller->state);
}

void wheel_pid_set_gains(wheel_pid_t *controller, float kp, float ki)
{
    controller->params.kp = kp;
    controller->params.ki = ki;
    if (ki == 0.0f)
    {
        controller->state.integral = 0.0f;
        controller->state.next_integral = 0.0f;
    }
}

float wheel_pid_prepare(wheel_pid_t *controller, float target_delta_counts,
                        int32_t measured_delta_counts)
{
    return pid_pi_prepare(&controller->state, &controller->params, target_delta_counts,
                          (float)measured_delta_counts);
}

void wheel_pid_track(wheel_pid_t *controller, int32_t applied_duty_permille, bool constrained)
{
    pid_pi_track(&controller->state, &controller->params, (float)WHEEL_PERIOD_MS / 1000.0f,
                 (float)applied_duty_permille, constrained);
}

void wheel_pid_get_status(const wheel_pid_t *controller, wheel_pid_status_t *status)
{
    *status = (wheel_pid_status_t){
        .params = controller->params,
        .error_counts = controller->state.error,
        .proportional_permille = controller->state.proportional,
        .integral_permille = controller->state.integral,
        .output_permille = controller->state.output,
        .next_integral_permille = controller->state.next_integral,
    };
}
