#include "pid.h"

void pid_pi_reset(pid_pi_state_t *state)
{
    *state = (pid_pi_state_t){0};
}

float pid_pi_prepare(pid_pi_state_t *state, const pid_pi_params_t *params, float target,
                     float measurement)
{
    state->error = target - measurement;
    state->proportional = params->kp * state->error;
    state->integral = params->ki > 0.0f ? state->next_integral : 0.0f;
    state->output = state->proportional + state->integral;
    return state->output;
}

void pid_pi_track(pid_pi_state_t *state, const pid_pi_params_t *params, float dt_s,
                  float applied_output, bool constrained)
{
    if (params->ki == 0.0f)
    {
        state->next_integral = 0.0f;
        return;
    }
    const float tracking =
        constrained ? params->tracking_gain_per_s * (applied_output - state->output) : 0.0f;
    state->next_integral = state->integral + dt_s * (params->ki * state->error + tracking);
}
