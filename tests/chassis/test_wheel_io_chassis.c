#include "runtime/wheel_io.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t ticks, micros;
static unsigned critical;
static motor_status_t motor;
static bool tripped;

void test_enter(void) { critical++; }
void test_exit(void) { assert(critical != 0U); critical--; }
uint32_t HAL_GetTick(void) { return ticks; }
uint32_t osKernelGetTickCount(void) { return ticks; }
uint32_t time_us_now(void) { return micros; }
bool encoder_init(void) { return true; }
void encoder_sample(encoder_sample_t *sample) { *sample = (encoder_sample_t){0}; }
void motor_init(uint32_t now) { (void)now; memset(&motor, 0, sizeof(motor)); tripped = false; }
void motor_stop(uint32_t now) { (void)now; memset(&motor, 0, sizeof(motor)); }
bool motor_guard_fault(void) { return tripped; }
void motor_guard_begin(uint32_t now) { (void)now; }
void motor_force_outputs_low(void) { memset(&motor, 0, sizeof(motor)); }
bool motor_update_with_brake(int16_t left, int16_t right, bool brake_left, bool brake_right,
                             uint32_t now)
{
    (void)now;
    motor.applied_left = left;
    motor.applied_right = right;
    motor.phase_left = left || brake_left ? MOTOR_RUNNING : MOTOR_OFF;
    motor.phase_right = right || brake_right ? MOTOR_RUNNING : MOTOR_OFF;
    return true;
}
bool motor_update(int16_t left, int16_t right, uint32_t now)
{
    return motor_update_with_brake(left, right, false, false, now);
}
void motor_get_status(motor_status_t *status) { *status = motor; }

static void step(void)
{
    ticks += 10U;
    micros += 10000U;
    wheel_io_step(ticks, false);
}

int main(void)
{
    assert(wheel_io_init(0U));
    assert(wheel_io_chassis_set(10.0f, 10.0f));
    step();
    assert(wheel_io_chassis_claimed());
    wheel_io_status_t status;
    wheel_io_get_status(&status);
    assert(status.session && status.motor.phase_left == MOTOR_RUNNING);
    assert(status.motor.applied_left > 0);

    speed_event_t event = {.type = SPEED_TARGET, .wheels = 1U, .target = {5.0f, 0.0f}};
    assert(!wheel_io_speed_enqueue(&event));
    assert(!wheel_io_request(50, 50, 100U));

    wheel_io_stop();
    step();
    assert(!wheel_io_chassis_claimed());
    wheel_io_get_status(&status);
    assert(status.motor.phase_left == MOTOR_OFF && status.motor.phase_right == MOTOR_OFF);
    assert(wheel_io_request(50, 50, 100U));
    step();
    wheel_io_get_status(&status);
    assert(status.motor.applied_left == 50);

    assert(critical == 0U);
    puts("PASS: wheel_io chassis claim, arbitration and stop");
    return 0;
}
