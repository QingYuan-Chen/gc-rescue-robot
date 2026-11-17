#include "chassis_kinematics.h"

#include "config/chassis_config.h"
#include "config/wheel_config.h"

#include <math.h>
#include <stddef.h>

#define CHASSIS_PI 3.14159265358979323846f

static float clampf(float value, float low, float high)
{
    if (value < low) { return low; }
    if (value > high) { return high; }
    return value;
}

bool chassis_kinematics(float v_mm_s, float omega_rad_s, chassis_wheel_targets_t *targets)
{
    if (targets == NULL || !isfinite(v_mm_s) || !isfinite(omega_rad_s))
    {
        return false;
    }

    const float mm_per_rpm = CHASSIS_PI * CHASSIS_WHEEL_DIAMETER_MM / 60.0f;
    float center_rpm = v_mm_s / mm_per_rpm;
    float turn_rpm = omega_rad_s * (CHASSIS_TRACK_WIDTH_MM * 0.5f) / mm_per_rpm;
    bool saturated = false;

    /* 转向优先：角速度最多用到两侧轮速的较小绝对值，再限制线速度。 */
    const float turn_limit = fminf(-WHEEL_TARGET_MIN_RPM, WHEEL_TARGET_MAX_RPM);
    const float limited_turn = clampf(turn_rpm, -turn_limit, turn_limit);
    if (limited_turn != turn_rpm) { saturated = true; }
    turn_rpm = limited_turn;

    const float low = WHEEL_TARGET_MIN_RPM + fabsf(turn_rpm);
    const float high = WHEEL_TARGET_MAX_RPM - fabsf(turn_rpm);
    const float limited_center = clampf(center_rpm, low, high);
    if (limited_center != center_rpm) { saturated = true; }
    center_rpm = limited_center;

    targets->left_rpm = center_rpm - turn_rpm;
    targets->right_rpm = center_rpm + turn_rpm;
    targets->saturated = saturated;
    return true;
}
