#include "heading_control.h"

#include <math.h>

#define HEADING_CONTROL_RAD_TO_DEG 57.29577951308232f
#define HEADING_CONTROL_MIN_DT_MS 1.0f
#define HEADING_CONTROL_MAX_DT_MS 100.0f

static float clampf(float value, float low, float high)
{
    if (value < low) { return low; }
    if (value > high) { return high; }
    return value;
}

/* 位置项：小偏差用线性 Kp·e，大偏差用减速度不超过 brake 的制动曲线。 */
static float position_command_dps(const heading_control_config_t *config, float error_deg)
{
    if (config->brake_dps2 <= 0.0f || config->kp <= 0.0f)
    {
        return config->kp * error_deg;
    }
    /* 两段在 |e| = e1 处值和斜率连续：e1 = brake/Kp²，e0 = e1/2。 */
    const float e1 = config->brake_dps2 / (config->kp * config->kp);
    const float magnitude = fabsf(error_deg);
    if (magnitude <= e1) { return config->kp * error_deg; }
    const float brake_command =
        sqrtf(2.0f * config->brake_dps2 * (magnitude - 0.5f * e1));
    return error_deg < 0.0f ? -brake_command : brake_command;
}

void heading_control_init(heading_control_t *control)
{
    *control = (heading_control_t){0};
}

bool heading_control_config_valid(const heading_control_config_t *config)
{
    /* 本文件启用了 -fno-fast-math，有界比较按 IEEE 规则自动拒绝 NaN 与 Inf。 */
    return config->kp >= 0.0f && config->kp <= 20.0f && config->kd >= 0.0f &&
           config->kd <= 5.0f && config->ki >= 0.0f && config->ki <= 2.0f &&
           config->filter_ms >= 0.0f && config->filter_ms <= 200.0f &&
           config->wmax_dps >= 1.0f && config->wmax_dps <= 200.0f &&
           config->accel_dps2 >= 10.0f && config->accel_dps2 <= 1000.0f &&
           config->tol_deg >= 0.1f && config->tol_deg <= 20.0f &&
           config->rate_tol_dps >= 0.1f && config->rate_tol_dps <= 50.0f &&
           config->izone_deg >= 0.1f && config->izone_deg <= 45.0f &&
           config->ilimit_dps >= 0.0f && config->ilimit_dps <= 50.0f &&
           config->brake_dps2 >= 0.0f && config->brake_dps2 <= config->accel_dps2 &&
           config->settle_ms >= 10U && config->settle_ms <= 1000U;
}

void heading_control_start(heading_control_t *control, float target_rad, uint32_t now_ms)
{
    control->target_rad = target_rad;
    control->last_omega_dps = 0.0f;
    control->filtered_rate_dps = 0.0f;
    control->integral_dps = 0.0f;
    control->last_ms = now_ms;
    control->settle_start_ms = 0U;
    control->settled = false;
}

float heading_control_step(heading_control_t *control, const heading_control_config_t *config,
                           float yaw_rad, float yaw_rate_rad_s, bool allow_integral,
                           uint32_t now_ms)
{
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

    const float p_dps = position_command_dps(config, error_deg);
    const float d_dps = config->kd * control->filtered_rate_dps;
    float unsaturated = p_dps + control->integral_dps - d_dps;

    /* 积分分离：大偏差不进积分；输出已饱和且误差与输出同向时不再同向积分。 */
    const bool saturated_against_error =
        fabsf(unsaturated) >= config->wmax_dps && error_deg * unsaturated > 0.0f;
    if (allow_integral && config->ki > 0.0f && fabsf(error_deg) <= config->izone_deg &&
        !saturated_against_error)
    {
        control->integral_dps += config->ki * error_deg * dt_s;
        control->integral_dps =
            clampf(control->integral_dps, -config->ilimit_dps, config->ilimit_dps);
        unsaturated = p_dps + control->integral_dps - d_dps;
    }

    float omega = clampf(unsaturated, -config->wmax_dps, config->wmax_dps);
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
