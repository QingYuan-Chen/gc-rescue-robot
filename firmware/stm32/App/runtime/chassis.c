#include "chassis.h"

#include "config/chassis_config.h"
#include "config/wheel_config.h"
#include "control/chassis_kinematics.h"
#include "runtime/heading.h"
#include "runtime/wheel_io.h"

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "task.h"

#include <math.h>

#define CHASSIS_PI 3.14159265358979323846f
#define CHASSIS_DEG_TO_RAD (CHASSIS_PI / 180.0f)
#define CHASSIS_RAD_TO_DEG (180.0f / CHASSIS_PI)

static chassis_status_t status;
static chassis_turn_config_t turn_config;
static chassis_hold_config_t hold_config;
static heading_control_t turn_control;
static heading_control_t hold_control;
static uint32_t started_ms, last_update_ms, duration_ms;
static int64_t distance_start_left, distance_start_right;

static chassis_turn_config_t default_turn_config(void)
{
    /* 原地转向整定值：Kd 用 0，靠制动曲线避免超调；转向不使用积分。 */
    return (chassis_turn_config_t){.kp = 3.0f,
                                   .kd = 0.0f,
                                   .ki = 0.0f,
                                   .filter_ms = 20.0f,
                                   .wmax_dps = 90.0f,
                                   .accel_dps2 = 180.0f,
                                   .tol_deg = 2.0f,
                                   .rate_tol_dps = 5.0f,
                                   .izone_deg = 5.0f,
                                   .ilimit_dps = 0.0f,
                                   .brake_dps2 = 100.0f,
                                   .settle_ms = 100U};
}

static chassis_hold_config_t default_hold_config(void)
{
    /* 直行保向整定值：D 项保留代码但取 0，用弱积分消除两轮速度差带来的稳态偏角。 */
    return (chassis_hold_config_t){.kp = 1.0f,
                                   .kd = 0.0f,
                                   .ki = 0.1f,
                                   .filter_ms = 20.0f,
                                   .wmax_dps = 30.0f,
                                   .accel_dps2 = 120.0f,
                                   .tol_deg = 3.0f,
                                   .rate_tol_dps = 10.0f,
                                   .izone_deg = 5.0f,
                                   .ilimit_dps = 5.0f,
                                   .brake_dps2 = 0.0f,
                                   .settle_ms = 100U};
}

static void finish(uint8_t reason)
{
    wheel_io_stop();
    status.mode = CHASSIS_IDLE;
    status.v_mm_s = 0.0f;
    status.omega_deg_s = 0.0f;
    status.remaining_ms = 0U;
    status.stop_reason = reason;
}

static void update_heading_status(const heading_status_t *heading)
{
    status.yaw_deg = heading->yaw_rad * CHASSIS_RAD_TO_DEG;
    status.yaw_error_deg = (status.target_yaw_deg - status.yaw_deg);
    status.yaw_rate_dps = heading->yaw_rate_rad_s * CHASSIS_RAD_TO_DEG;
    status.imu_valid = heading->valid;
}

static void update_distance(void)
{
    wheel_io_status_t wheel;
    wheel_io_get_status(&wheel);
    const int64_t counts = ((wheel.encoder.total_left - distance_start_left) +
                            (wheel.encoder.total_right - distance_start_right)) / 2;
    status.distance_mm = (float)counts * (CHASSIS_PI * CHASSIS_WHEEL_DIAMETER_MM) /
                         WHEEL_ENCODER_COUNTS_PER_REV;
}

static bool update_targets(float v_mm_s, float omega_deg_s)
{
    chassis_wheel_targets_t targets;
    chassis_kinematics(v_mm_s, omega_deg_s * CHASSIS_DEG_TO_RAD, &targets);
    if (!wheel_io_chassis_set(targets.left_rpm, targets.right_rpm)) { return false; }
    status.left_rpm = targets.left_rpm;
    status.right_rpm = targets.right_rpm;
    status.saturated = targets.saturated;
    return true;
}

static void update_turn_status(const heading_status_t *heading, float omega_deg_s)
{
    update_heading_status(heading);
    status.omega_deg_s = omega_deg_s;
    status.target_yaw_deg = turn_control.target_rad * CHASSIS_RAD_TO_DEG;
    status.settled = turn_control.settled;
}

void chassis_init(void)
{
    status = (chassis_status_t){0};
    turn_config = default_turn_config();
    hold_config = default_hold_config();
    heading_control_init(&turn_control);
    heading_control_init(&hold_control);
    started_ms = last_update_ms = duration_ms = 0U;
    distance_start_left = distance_start_right = 0;
}

bool chassis_start_vw(float v_mm_s, float omega_deg_s, uint32_t duration_ms_arg)
{
    /* 命令上限：线速度 300 mm/s，差速角速度 120°/s。 */
    if (!isfinite(v_mm_s) || !isfinite(omega_deg_s) ||
        fabsf(v_mm_s) > 300.0f || fabsf(omega_deg_s) > 120.0f ||
        duration_ms_arg < CHASSIS_COMMAND_MIN_MS ||
        duration_ms_arg > CHASSIS_COMMAND_MAX_MS)
    {
        return false;
    }
    status = (chassis_status_t){.mode = CHASSIS_VW, .v_mm_s = v_mm_s,
                                .omega_deg_s = omega_deg_s};
    if (!update_targets(v_mm_s, omega_deg_s))
    {
        status = (chassis_status_t){0};
        return false;
    }
    started_ms = osKernelGetTickCount();
    last_update_ms = started_ms;
    duration_ms = duration_ms_arg;
    status.remaining_ms = duration_ms;
    return true;
}

bool chassis_start_turn(float relative_deg)
{
    /* 转向角度上限 360°。 */
    if (!isfinite(relative_deg) || fabsf(relative_deg) < 0.1f ||
        fabsf(relative_deg) > 360.0f)
    {
        return false;
    }
    heading_status_t heading;
    heading_get_status(&heading);
    if (!heading.valid) { return false; }

    status = (chassis_status_t){.mode = CHASSIS_TURN};
    status.target_yaw_deg = (heading.yaw_rad + relative_deg * CHASSIS_DEG_TO_RAD) *
                            CHASSIS_RAD_TO_DEG;
    heading_control_start(&turn_control, status.target_yaw_deg * CHASSIS_DEG_TO_RAD,
                          osKernelGetTickCount());
    if (!update_targets(0.0f, 0.0f))
    {
        status = (chassis_status_t){0};
        return false;
    }
    started_ms = last_update_ms = osKernelGetTickCount();
    duration_ms = 5000U; /* 转向超时 5 s。 */
    status.remaining_ms = duration_ms;
    update_turn_status(&heading, 0.0f);
    return true;
}

bool chassis_start_straight(float v_mm_s, uint32_t duration_ms_arg, float distance_mm)
{
    /* 直行线速度上限 300 mm/s，可选里程上限 10000 mm。 */
    if (!isfinite(v_mm_s) || fabsf(v_mm_s) > 300.0f ||
        duration_ms_arg < CHASSIS_COMMAND_MIN_MS ||
        duration_ms_arg > CHASSIS_COMMAND_MAX_MS || !isfinite(distance_mm) ||
        distance_mm < 0.0f || distance_mm > 10000.0f)
    {
        return false;
    }
    heading_status_t heading;
    heading_get_status(&heading);
    if (!heading.valid) { return false; }

    status = (chassis_status_t){.mode = CHASSIS_STRAIGHT, .v_mm_s = v_mm_s,
                                .distance_target_mm = distance_mm};
    status.target_yaw_deg = heading.yaw_rad * CHASSIS_RAD_TO_DEG;
    heading_control_start(&hold_control, heading.yaw_rad, osKernelGetTickCount());
    wheel_io_status_t wheel;
    wheel_io_get_status(&wheel);
    distance_start_left = wheel.encoder.total_left;
    distance_start_right = wheel.encoder.total_right;
    if (!update_targets(v_mm_s, 0.0f))
    {
        status = (chassis_status_t){0};
        return false;
    }
    started_ms = last_update_ms = osKernelGetTickCount();
    duration_ms = duration_ms_arg;
    status.remaining_ms = duration_ms;
    update_heading_status(&heading);
    return true;
}

void chassis_stop(void)
{
    if (status.mode != CHASSIS_IDLE)
    {
        finish(4U);
    }
}

void chassis_step(uint32_t now_ms)
{
    if (status.mode == CHASSIS_IDLE) { return; }
    if (!wheel_io_chassis_claimed())
    {
        status = (chassis_status_t){0};
        return;
    }
    const uint32_t elapsed = now_ms - started_ms;
    if (elapsed >= duration_ms)
    {
        finish(status.mode == CHASSIS_TURN ? 2U : 1U);
        return;
    }
    status.remaining_ms = duration_ms - elapsed;
    /* 底盘控制周期 20 ms。 */
    if (now_ms - last_update_ms < 20U) { return; }
    last_update_ms = now_ms;

    if (status.mode == CHASSIS_VW)
    {
        (void)update_targets(status.v_mm_s, status.omega_deg_s);
        return;
    }

    heading_status_t heading;
    heading_get_status(&heading);
    if (!heading.valid)
    {
        finish(3U);
        return;
    }

    if (status.mode == CHASSIS_TURN)
    {
        const float omega = heading_control_step(&turn_control, &turn_config, heading.yaw_rad,
                                                 heading.yaw_rate_rad_s, false, now_ms);
        (void)update_targets(0.0f, omega);
        update_turn_status(&heading, omega);
        if (turn_control.settled) { finish(1U); }
        return;
    }

    if (status.mode == CHASSIS_STRAIGHT)
    {
        update_distance();
        float v_cmd = status.v_mm_s;
        if (status.distance_target_mm > 0.0f)
        {
            const float remaining = status.distance_target_mm - fabsf(status.distance_mm);
            if (remaining <= 0.0f)
            {
                finish(1U);
                return;
            }
            /* 带里程时按 300 mm/s² 提前减速。 */
            const float allowed = sqrtf(2.0f * 300.0f * remaining);
            if (fabsf(v_cmd) > allowed) { v_cmd = v_cmd > 0.0f ? allowed : -allowed; }
        }
        /* 起步 300 ms 内、或线速度低于 30 mm/s 时冻结积分。 */
        const bool integral_allowed = fabsf(v_cmd) >= 30.0f &&
                                      (now_ms - started_ms) >= 300U;
        const float omega = heading_control_step(&hold_control, &hold_config, heading.yaw_rad,
                                                 heading.yaw_rate_rad_s, integral_allowed,
                                                 now_ms);
        (void)update_targets(v_cmd, omega);
        update_heading_status(&heading);
        status.omega_deg_s = omega;
        status.i_dps = hold_control.integral_dps;
        status.settled = hold_control.settled;
    }
}

void chassis_get_status(chassis_status_t *out)
{
    taskENTER_CRITICAL();
    *out = status;
    taskEXIT_CRITICAL();
}

bool chassis_set_turn_config(const chassis_turn_config_t *config)
{
    if (!heading_control_config_valid(config)) { return false; }
    taskENTER_CRITICAL();
    turn_config = *config;
    taskEXIT_CRITICAL();
    return true;
}

bool chassis_set_hold_config(const chassis_hold_config_t *config)
{
    if (!heading_control_config_valid(config)) { return false; }
    taskENTER_CRITICAL();
    hold_config = *config;
    taskEXIT_CRITICAL();
    return true;
}

void chassis_get_turn_config(chassis_turn_config_t *config)
{
    taskENTER_CRITICAL();
    *config = turn_config;
    taskEXIT_CRITICAL();
}

void chassis_get_hold_config(chassis_hold_config_t *config)
{
    taskENTER_CRITICAL();
    *config = hold_config;
    taskEXIT_CRITICAL();
}
