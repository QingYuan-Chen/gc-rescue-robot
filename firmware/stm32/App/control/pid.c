#include "pid.h"

void pid_incremental_reset(pid_incremental_state_t *state)
{
    *state = (pid_incremental_state_t){0};
}

void pid_positional_reset(pid_positional_state_t *state)
{
    *state = (pid_positional_state_t){0};
}

float pid_incremental_step(pid_incremental_state_t *state, const pid_params_t *params, float target,
                           float measurement)
{
    state->target = target;
    state->measurement = measurement;
    state->error = target - measurement;

    const float proportion = state->error - state->previous_error;
    float integral = 0.0f;
    /* 原实现使用严格越界判断，恰在阈值处仍允许积分增量。 */
    if (state->output > params->output_max)
    {
        if (state->error < 0.0f)
        {
            integral = state->error;
        }
    }
    else if (state->output < -params->output_max)
    {
        if (state->error > 0.0f)
        {
            integral = state->error;
        }
    }
    else
    {
        integral = state->error;
    }

    const float differential = state->error - 2.0f * state->previous_error + state->previous2_error;
    state->output += params->kp * proportion + params->ki * integral + params->kd * differential;
    state->previous2_error = state->previous_error;
    state->previous_error = state->error;
    return state->output;
}

float pid_positional_step(pid_positional_state_t *state, const pid_params_t *params, float target,
                          float measurement)
{
    state->target = target;
    state->measurement = measurement;
    state->error = target - measurement;

    if (state->output >= params->output_max)
    {
        if (state->error < 0.0f)
        {
            state->integral += state->error;
        }
    }
    else if (state->output <= params->output_min)
    {
        if (state->error > 0.0f)
        {
            state->integral += state->error;
        }
    }
    else
    {
        state->integral += state->error;
    }

    const float differential =
        (state->error - state->previous_error) * params->differential_filter +
        (1.0f - params->differential_filter) * state->previous_differential;
    state->output =
        params->kp * state->error + params->ki * state->integral + params->kd * differential;
    if (state->output > params->output_max)
    {
        state->output = params->output_max;
    }
    else if (state->output < params->output_min)
    {
        state->output = params->output_min;
    }
    state->previous_error = state->error;
    state->previous_differential = differential;
    return state->output;
}
