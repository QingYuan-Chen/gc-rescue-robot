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
    return (chassis_turn_config_t){.kp = CHASSIS_TURN_KP,
                                   .kd = CHASSIS_TURN_KD,
                                   .filter_ms = CHASSIS_TURN_FILTER_MS,
                                   .wmax_dps = CHASSIS_TURN_WMAX_DPS,
                                   .accel_dps2 = CHASSIS_TURN_ACCEL_DPS2,
                                   .tol_deg = CHASSIS_TURN_TOL_DEG,
                                   .rate_tol_dps = CHASSIS_TURN_RATE_TOL_DPS,
                                   .settle_ms = CHASSIS_TURN_SETTLE_MS};
}

static chassis_hold_config_t default_hold_config(void)
{
    return (chassis_hold_config_t){.kp = CHASSIS_HOLD_KP,
                                   .kd = CHASSIS_HOLD_KD,
                                   .filter_ms = CHASSIS_HOLD_FILTER_MS,
                                   .wmax_dps = CHASSIS_HOLD_WMAX_DPS,
                                   .accel_dps2 = CHASSIS_HOLD_ACCEL_DPS2,
                                   .tol_deg = CHASSIS_HOLD_TOL_DEG,
                                   .rate_tol_dps = CHASSIS_HOLD_RATE_TOL_DPS,
                                   .settle_ms = CHASSIS_HOLD_SETTLE_MS};
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
    if (!chassis_kinematics(v_mm_s, omega_deg_s * CHASSIS_DEG_TO_RAD, &targets))
    {
        return false;
    }
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
    if (!isfinite(v_mm_s) || !isfinite(omega_deg_s) ||
        fabsf(v_mm_s) > CHASSIS_MAX_V_MM_S || fabsf(omega_deg_s) > CHASSIS_MAX_OMEGA_DEG_S ||
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
    if (!isfinite(relative_deg) || fabsf(relative_deg) < 0.1f ||
        fabsf(relative_deg) > CHASSIS_TURN_MAX_DEG)
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
    duration_ms = CHASSIS_TURN_TIMEOUT_MS;
    status.remaining_ms = duration_ms;
    update_turn_status(&heading, 0.0f);
    return true;
}

bool chassis_start_straight(float v_mm_s, uint32_t duration_ms_arg, float distance_mm)
{
    if (!isfinite(v_mm_s) || fabsf(v_mm_s) > CHASSIS_MAX_V_MM_S ||
        duration_ms_arg < CHASSIS_COMMAND_MIN_MS ||
        duration_ms_arg > CHASSIS_COMMAND_MAX_MS || !isfinite(distance_mm) ||
        distance_mm < 0.0f || distance_mm > CHASSIS_MAX_DISTANCE_MM)
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
    else
    {
        status = (chassis_status_t){0};
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
    if (now_ms - last_update_ms < CHASSIS_PERIOD_MS) { return; }
    last_update_ms = now_ms;

    if (status.mode == CHASSIS_VW)
    {
        if (!update_targets(status.v_mm_s, status.omega_deg_s)) { finish(5U); }
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
                                                 heading.yaw_rate_rad_s, now_ms);
        if (!update_targets(0.0f, omega))
        {
            finish(5U);
            return;
        }
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
            const float allowed = sqrtf(2.0f * CHASSIS_STRAIGHT_ACCEL_MM_S2 * remaining);
            if (fabsf(v_cmd) > allowed) { v_cmd = v_cmd > 0.0f ? allowed : -allowed; }
        }
        const float omega = heading_control_step(&hold_control, &hold_config, heading.yaw_rad,
                                                 heading.yaw_rate_rad_s, now_ms);
        if (!update_targets(v_cmd, omega))
        {
            finish(5U);
            return;
        }
        update_heading_status(&heading);
        status.omega_deg_s = omega;
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
