#include "heading.h"

#include "bsp/imu.h"
#include "config/chassis_config.h"

#include "FreeRTOS.h"
#include "task.h"

#include <math.h>

#define HEADING_PI 3.14159265358979323846f
/* JY61P 角度原始值按 ±180° 映射到 int16，角速度按 ±2000°/s 映射。 */
#define HEADING_ANGLE_RAD_PER_LSB (HEADING_PI / 32768.0f)
#define HEADING_RATE_RAD_PER_LSB (2000.0f * HEADING_PI / 180.0f / 32768.0f)

static heading_status_t latest;
static float last_raw_rad;
static uint32_t last_angle_ms;
static bool has_angle;

static float wrap_pi(float value)
{
    while (value > HEADING_PI) { value -= 2.0f * HEADING_PI; }
    while (value < -HEADING_PI) { value += 2.0f * HEADING_PI; }
    return value;
}

void heading_init(void)
{
    latest = (heading_status_t){0};
    last_raw_rad = 0.0f;
    last_angle_ms = 0U;
    has_angle = false;
}

void heading_update(uint32_t now_ms)
{
    imu_status_t imu;
    imu_get_status(&imu);
    latest.jump_error = false;

    if (imu.angle_frames != latest.angle_frames)
    {
        const float raw = CHASSIS_IMU_YAW_SIGN * (float)imu.angle[CHASSIS_IMU_YAW_AXIS];
        const float raw_rad = raw * HEADING_ANGLE_RAD_PER_LSB;
        if (!has_angle)
        {
            latest.yaw_rad = 0.0f;
            has_angle = true;
        }
        else
        {
            const float delta = wrap_pi(raw_rad - last_raw_rad);
            if (fabsf(delta) <= CHASSIS_IMU_MAX_DELTA_RAD)
            {
                latest.yaw_rad += delta;
            }
            else
            {
                latest.jump_error = true;
            }
        }
        if (last_angle_ms != 0U)
        {
            latest.frame_period_ms = imu.angle_ms - last_angle_ms;
        }
        last_angle_ms = imu.angle_ms;
        last_raw_rad = raw_rad;
        latest.angle_frames = imu.angle_frames;
    }

    if (imu.gyro_frames != latest.gyro_frames)
    {
        const float raw = CHASSIS_IMU_RATE_SIGN * (float)imu.gyro[CHASSIS_IMU_YAW_AXIS];
        latest.yaw_rate_rad_s = raw * HEADING_RATE_RAD_PER_LSB;
        latest.gyro_frames = imu.gyro_frames;
    }

    latest.age_ms = now_ms - imu.angle_ms;
    const uint32_t gyro_age = now_ms - imu.gyro_ms;
    latest.valid = has_angle && !latest.jump_error && imu.receiving &&
                   imu.angle_frames != 0U && imu.gyro_frames != 0U &&
                   latest.age_ms <= CHASSIS_IMU_TIMEOUT_MS &&
                   gyro_age <= CHASSIS_IMU_TIMEOUT_MS;
}

void heading_get_status(heading_status_t *status)
{
    taskENTER_CRITICAL();
    *status = latest;
    taskEXIT_CRITICAL();
}

bool heading_valid(void)
{
    taskENTER_CRITICAL();
    const bool valid = latest.valid;
    taskEXIT_CRITICAL();
    return valid;
}
