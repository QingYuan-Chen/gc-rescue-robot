#include "runtime/wheel_io.h"
#include "debug/position_commands.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint32_t tick, us;
static unsigned critical;
static bool guard_fault;
static int64_t total[2], previous[2];
static motor_status_t motor;

void test_enter(void) { critical++; }
void test_exit(void) { assert(critical != 0U); critical--; }
uint32_t HAL_GetTick(void) { return tick; }
uint32_t osKernelGetTickCount(void) { return tick; }
uint32_t time_us_now(void) { return us; }
bool encoder_init(void) { return true; }
void encoder_sample(encoder_sample_t *sample)
{
    *sample = (encoder_sample_t){.total_left = total[0], .total_right = total[1],
        .delta_left = (int32_t)(total[0] - previous[0]),
        .delta_right = (int32_t)(total[1] - previous[1])};
    previous[0] = total[0]; previous[1] = total[1];
}
void motor_init(uint32_t now) { (void)now; motor = (motor_status_t){0}; guard_fault = false; }
void motor_stop(uint32_t now) { (void)now; motor = (motor_status_t){0}; }
bool motor_guard_fault(void) { return guard_fault; }
void motor_guard_begin(uint32_t now) { (void)now; }
void motor_force_outputs_low(void) { motor = (motor_status_t){0}; }
bool motor_update_with_brake(int16_t left, int16_t right, bool brake_left, bool brake_right,
                             uint32_t now)
{
    (void)now;
    motor.applied_left = left; motor.applied_right = right;
    motor.phase_left = left || brake_left ? MOTOR_RUNNING : MOTOR_OFF;
    motor.phase_right = right || brake_right ? MOTOR_RUNNING : MOTOR_OFF;
    return true;
}
bool motor_update(int16_t left, int16_t right, uint32_t now)
{
    return motor_update_with_brake(left, right, false, false, now);
}
void motor_get_status(motor_status_t *status) { *status = motor; }

static void step(void) { tick += 10U; us += 10000U; wheel_io_step(tick, false); }
static wheel_position_status_t pos(void)
{
    wheel_position_status_t result; wheel_io_position_get(&result); return result;
}
static void init(void)
{
    tick = us = 0; critical = 0; total[0] = 5000; total[1] = -7000;
    previous[0] = total[0]; previous[1] = total[1];
    assert(wheel_io_init(0));
}
static void settle(void) { for (unsigned i = 0; i < 30; i++) { step(); } }
static void start(int32_t left, int32_t right)
{
    uint32_t id;
    assert(wheel_io_position_move(left, right, &id));
    assert(id != 0 && pos().state == POSITION_QUEUED);
    step();
    assert(pos().state == POSITION_RUNNING);
}

int main(void)
{
    uint32_t id;
    init();
    assert(!wheel_io_position_move(0, 0, &id));
    assert(!wheel_io_position_move(1981, 0, &id));
    assert(wheel_io_request(100, 0, 100));
    assert(!wheel_io_position_move(10, 0, &id));
    wheel_io_stop(); step();
    start(198, 0);
    assert(pos().target_rpm[0] == 20.0f && pos().target_rpm[1] == 0.0f);
    assert(motor.applied_left > 0 && motor.phase_right == MOTOR_OFF);
    assert(!wheel_io_request(100, 0, 100));
    assert(!wheel_io_position_move(1, 0, &id));
    assert(!wheel_io_speed_stage(0, SPEED_PARAM_KP, 4.0f, true, &id));
    speed_event_t event = {.type = SPEED_TARGET, .wheels = 1, .target = {10.0f, 0.0f}};
    assert(!wheel_io_speed_enqueue(&event));
    event.type = SPEED_APPLY;
    assert(!wheel_io_speed_enqueue(&event));
    total[0] += 190; step(); settle();
    assert(pos().state == POSITION_DONE && pos().actual[0] == 190 && pos().error[0] == 8);
    assert(motor.phase_left == MOTOR_OFF && motor.phase_right == MOTOR_OFF);
    total[0] += 20; step();
    assert(pos().state == POSITION_DONE && motor.phase_left == MOTOR_OFF);

    init(); start(-198, 100);
    assert(pos().target_rpm[0] == -20.0f && fabsf(pos().target_rpm[1] - 15.0f) < 0.001f);
    total[0] -= 198; total[1] += 96; step(); settle();
    assert(pos().state == POSITION_DONE && pos().actual[0] == -198 && pos().actual[1] == 96);

    init(); start(100, 0);
    total[0] += 130; step(); settle();
    assert(pos().state == POSITION_RUNNING && pos().corrections[0] == 1);
    assert(pos().target_rpm[0] < 0.0f);
    total[0] -= 25; step(); settle();
    assert(pos().state == POSITION_DONE && pos().error[0] == -5);

    init(); start(100, 0);
    total[0] += 130; step(); settle();
    total[0] -= 60; step(); settle();
    assert(pos().state == POSITION_OVERSHOOT && motor.phase_left == MOTOR_OFF);

    init(); start(100, 0); settle();
    for (unsigned i = 0; i < 1000; i++) { step(); }
    assert(pos().state == POSITION_TIMEOUT && pos().elapsed_ms == 10000U);
    assert(motor.phase_left == MOTOR_OFF);
    init(); start(100, 0); wheel_io_stop(); step();
    assert(pos().state == POSITION_STOPPED && motor.phase_left == MOTOR_OFF);
    init(); assert(wheel_io_position_move(100, 0, &id)); wheel_io_stop(); step();
    assert(pos().state == POSITION_STOPPED && motor.phase_left == MOTOR_OFF);
    init(); start(100, 0); guard_fault = true; step();
    assert(pos().state == POSITION_FAULT && motor.phase_left == MOTOR_OFF);

    init();
    char response[512], command[] = "pos move -198 100";
    assert(position_commands_execute(command, response, sizeof(response)));
    assert(strstr(response, "OK pos queued id=") != NULL);
    step();
    char get[] = "pos get";
    assert(position_commands_execute(get, response, sizeof(response)));
    assert(strstr(response, "state=running") && strstr(response, "target=-198,100"));
    char bad[] = "pos move 12x 0";
    assert(position_commands_execute(bad, response, sizeof(response)));
    assert(strstr(response, "ERR") != NULL);
    init();
    char slider_zero[] = "pos target let:0";
    assert(position_commands_execute(slider_zero, response, sizeof(response)));
    assert(strstr(response, "OK pos target_left=0 id=0") != NULL);
    char slider_target[] = "pos target let:198.000000";
    assert(position_commands_execute(slider_target, response, sizeof(response)));
    assert(strstr(response, "OK pos target_left=198 id=") != NULL);
    step();
    assert(pos().target[0] == 198 && pos().target[1] == 0);
    char retarget[] = "pos target let:120";
    assert(position_commands_execute(retarget, response, sizeof(response)));
    assert(strstr(response, "OK pos target_left=120 id=") != NULL);
    step();
    assert(pos().target[0] == 120 && pos().target[1] == 0);
    wheel_io_stop(); step();
    init();
    char slider_ki[] = "pos set ki:0.01";
    assert(position_commands_execute(slider_ki, response, sizeof(response)));
    assert(strstr(response, "ki_millionths=10000") != NULL);
    char slider_kd[] = "pos set kd:0.02";
    assert(position_commands_execute(slider_kd, response, sizeof(response)));
    assert(strstr(response, "kd_millionths=20000") != NULL);
    assert(critical == 0);
    puts("PASS: position move, correction, arbitration, stopping and faults");
}
