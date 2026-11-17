#include "runtime/chassis.h"
#include "runtime/heading.h"
#include "runtime/wheel_io.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t tick;
static unsigned critical;
static bool claimed;
static bool busy;
static float left_rpm, right_rpm;
static heading_status_t heading;
static wheel_io_status_t wheel;

void test_enter(void) { critical++; }
void test_exit(void) { assert(critical != 0U); critical--; }
uint32_t osKernelGetTickCount(void) { return tick; }
bool wheel_io_chassis_set(float left, float right)
{
    if (busy && !claimed) { return false; }
    claimed = true;
    left_rpm = left;
    right_rpm = right;
    return true;
}
bool wheel_io_chassis_claimed(void) { return claimed; }
void wheel_io_stop(void) { claimed = false; }
void wheel_io_get_status(wheel_io_status_t *status) { *status = wheel; }
void heading_get_status(heading_status_t *status) { *status = heading; }

int main(void)
{
    chassis_init();
    tick = 0U;
    memset(&wheel, 0, sizeof(wheel));
    heading = (heading_status_t){.valid = true};
    assert(chassis_start_vw(100.0f, 0.0f, 100U));
    assert(claimed);
    assert(left_rpm > 29.0f && left_rpm < 30.0f);
    assert(fabsf(left_rpm - right_rpm) < 0.01f);

    chassis_status_t status;
    chassis_get_status(&status);
    assert(status.mode == CHASSIS_VW && status.remaining_ms == 100U);

    tick = 20U;
    chassis_step(tick);
    chassis_get_status(&status);
    assert(status.mode == CHASSIS_VW && status.remaining_ms == 80U);

    tick = 100U;
    chassis_step(tick);
    chassis_get_status(&status);
    assert(status.mode == CHASSIS_IDLE && !claimed);

    assert(chassis_start_vw(200.0f, 120.0f, 200U));
    chassis_get_status(&status);
    assert(status.saturated);
    assert(fabsf(status.right_rpm - 97.0f) < 0.1f);
    chassis_stop();
    assert(!claimed);

    tick = 300U;
    assert(chassis_start_turn(90.0f));
    assert(claimed);
    chassis_get_status(&status);
    assert(status.mode == CHASSIS_TURN);
    assert(fabsf(status.target_yaw_deg - 90.0f) < 0.1f);
    tick = 320U;
    chassis_step(tick);
    chassis_get_status(&status);
    assert(status.mode == CHASSIS_TURN);
    assert(status.omega_deg_s > 0.0f);

    tick = 400U;
    assert(chassis_start_straight(100.0f, 200U, 500.0f));
    chassis_get_status(&status);
    assert(status.mode == CHASSIS_STRAIGHT);
    assert(fabsf(status.distance_target_mm - 500.0f) < 0.1f);
    tick = 420U;
    chassis_step(tick);
    chassis_get_status(&status);
    assert(status.mode == CHASSIS_STRAIGHT);
    assert(status.left_rpm > 0.0f);
    chassis_stop();
    assert(!claimed);

    /* 直行保向：起步 300 ms 内冻结积分，之后偏差进入积分。 */
    tick = 500U;
    heading.yaw_rad = 0.0f;
    heading.yaw_rate_rad_s = 0.0f;
    assert(chassis_start_straight(100.0f, 2000U, 0.0f));
    heading.yaw_rad = -0.0174533f; /* 偏 1°，误差进入积分区间 */
    tick = 600U;
    chassis_step(tick);
    chassis_get_status(&status);
    assert(status.mode == CHASSIS_STRAIGHT);
    assert(status.i_dps == 0.0f);

    tick = 820U;
    chassis_step(tick);
    chassis_get_status(&status);
    assert(status.i_dps > 0.0f);
    chassis_stop();

    /* 线速度低于 30 mm/s 时冻结积分。 */
    tick = 900U;
    heading.yaw_rad = 0.0f;
    assert(chassis_start_straight(20.0f, 2000U, 0.0f));
    heading.yaw_rad = -0.0174533f;
    tick = 1300U;
    chassis_step(tick);
    chassis_get_status(&status);
    assert(status.mode == CHASSIS_STRAIGHT);
    assert(status.i_dps == 0.0f);
    chassis_stop();

    busy = true;
    assert(!chassis_start_vw(100.0f, 0.0f, 100U));
    assert(!chassis_start_vw(500.0f, 0.0f, 100U));
    assert(chassis_start_vw(100.0f, 0.0f, 5U) == false);

    assert(critical == 0U);
    puts("PASS: chassis vw claim, expiry, saturation and rejection");
    return 0;
}
