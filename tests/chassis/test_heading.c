#include "runtime/heading.h"
#include "bsp/imu.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static imu_status_t imu;
static unsigned critical;
static uint32_t now_ms;

void test_enter(void) { critical++; }
void test_exit(void) { assert(critical != 0U); critical--; }
uint32_t HAL_GetTick(void) { return now_ms; }
void imu_get_status(imu_status_t *status) { *status = imu; }

static void frame(int16_t yaw_raw)
{
    imu.angle[2] = yaw_raw;
    imu.gyro[2] = 0;
    imu.angle_frames++;
    imu.gyro_frames++;
    imu.angle_ms = now_ms;
    imu.gyro_ms = now_ms;
    imu.receiving = true;
    heading_update(now_ms);
    now_ms += 10U;
}

int main(void)
{
    memset(&imu, 0, sizeof(imu));
    now_ms = 10U;
    heading_init();

    frame(0);
    heading_status_t status;
    heading_get_status(&status);
    assert(fabsf(status.yaw_rad) < 0.001f);
    assert(status.valid);

    /* 每帧 20°，累计一圈；跨 ±180° 时航向仍连续。 */
    for (int angle = 20; angle <= 360; angle += 20)
    {
        const int16_t raw = (int16_t)lroundf((float)angle * 32768.0f / 180.0f);
        frame(raw);
    }
    heading_get_status(&status);
    assert(fabsf(status.yaw_rad - 6.2831853f) < 0.02f);
    assert(status.frame_period_ms == 10U);

    heading_init();
    frame(0);
    frame(20000);
    heading_get_status(&status);
    assert(status.jump_error);
    assert(fabsf(status.yaw_rad) < 0.001f);

    heading_init();
    frame(0);
    now_ms += 51U;
    heading_update(now_ms);
    heading_get_status(&status);
    assert(!status.valid);

    assert(critical == 0U);
    puts("PASS: heading unwrap, jump detection and timeout");
    return 0;
}
