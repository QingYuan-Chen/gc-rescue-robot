#include "heading_control.h"

#include <math.h>
#include <stddef.h>

#define HEADING_CONTROL_RAD_TO_DEG 57.29577951308232f
#define HEADING_CONTROL_MIN_DT_MS 1.0f
#define HEADING_CONTROL_MAX_DT_MS 100.0f

static float clampf(float value, float low, float high)
{
    if (value < low) { return low; }
    if (value > high) { return high; }
    return value;
}

void heading_control_init(heading_control_t *control)
{
    *control = (heading_control_t){0};
}

bool heading_control_config_valid(const heading_control_config_t *config)
{
    if (config == NULL) { return false; }
    return isfinite(config->kp) && isfinite(config->kd) && isfinite(config->filter_ms) &&
           isfinite(config->wmax_dps) && isfinite(config->accel_dps2) &&
           isfinite(config->tol_deg) && isfinite(config->rate_tol_dps) &&
           config->kp >= 0.0f && config->kp <= 20.0f && config->kd >= 0.0f &&
           config->kd <= 5.0f && config->filter_ms >= 0.0f &&
           config->filter_ms <= 200.0f && config->wmax_dps >= 1.0f &&
           config->wmax_dps <= 200.0f && config->accel_dps2 >= 10.0f &&
           config->accel_dps2 <= 1000.0f && config->tol_deg >= 0.1f &&
           config->tol_deg <= 20.0f && config->rate_tol_dps >= 0.1f &&
           config->rate_tol_dps <= 50.0f && config->settle_ms >= 10U &&
           config->settle_ms <= 1000U;
}

void heading_control_start(heading_control_t *control, float target_rad, uint32_t now_ms)
{
    control->target_rad = target_rad;
    control->last_omega_dps = 0.0f;
    control->filtered_rate_dps = 0.0f;
    control->last_ms = now_ms;
    control->settle_start_ms = 0U;
    control->started = true;
    control->settled = false;
}

float heading_control_step(heading_control_t *control, const heading_control_config_t *config,
                           float yaw_rad, float yaw_rate_rad_s, uint32_t now_ms)
{
    if (!control->started) { return 0.0f; }
    const float dt_ms = clampf((float)(now_ms - control->last_ms),
                               HEADING_CONTROL_MIN_DT_MS, HEADING_CONTROL_MAX_DT_MS);
    const float dt_s = dt_ms * 0.001f;
    control->last_ms = now_ms;

    const float error_deg = (control->target_rad - yaw_rad) * HEADING_CONTROL_RAD_TO_DEG;
    const float rate_dps = yaw_rate_rad_s * HEADING_CONTROL_RAD_TO_DEG;
    const float alpha = config->filter_ms > 0.0f
                            ? config->filter_ms / (config->filter_ms + dt_ms) : 0.0f;
    control->filtered_rate_dps =
        alpha * control->filtered_rate_dps + (1.0f - alpha) * rate_dps;

    float omega = config->kp * error_deg - config->kd * control->filtered_rate_dps;
    omega = clampf(omega, -config->wmax_dps, config->wmax_dps);
    const float max_delta = config->accel_dps2 * dt_s;
    control->last_omega_dps += clampf(omega - control->last_omega_dps, -max_delta, max_delta);
    omega = control->last_omega_dps;

    const bool in_tolerance = fabsf(error_deg) <= config->tol_deg &&
                              fabsf(control->filtered_rate_dps) <= config->rate_tol_dps;
    if (in_tolerance)
    {
        if (control->settle_start_ms == 0U) { control->settle_start_ms = now_ms; }
        if (now_ms - control->settle_start_ms >= config->settle_ms)
        {
            control->settled = true;
        }
    }
    else
    {
        control->settle_start_ms = 0U;
        control->settled = false;
    }
    return omega;
}
