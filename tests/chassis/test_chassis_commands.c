#include "debug/chassis_commands.h"
#include "runtime/chassis.h"
#include "runtime/heading.h"
#include "runtime/wheel_io.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint32_t tick;
static unsigned critical;
static bool claimed;
static heading_status_t heading;
static wheel_io_status_t wheel;

void test_enter(void) { critical++; }
void test_exit(void) { assert(critical != 0U); critical--; }
uint32_t osKernelGetTickCount(void) { return tick; }
bool wheel_io_chassis_set(float left, float right)
{
    (void)left;
    (void)right;
    claimed = true;
    return true;
}
bool wheel_io_chassis_claimed(void) { return claimed; }
void wheel_io_stop(void) { claimed = false; }
void wheel_io_get_status(wheel_io_status_t *status) { *status = wheel; }
void heading_get_status(heading_status_t *status) { *status = heading; }

static void command(char *line, const char *expected)
{
    char response[1024];
    assert(chassis_commands_execute(line, response, sizeof(response)));
    assert(strstr(response, expected) != NULL);
}

int main(void)
{
    chassis_init();
    heading = (heading_status_t){.valid = true};
    tick = 0U;
    char vw[] = "chassis vw 100 0 100";
    command(vw, "OK chassis mode=1");
    char turn[] = "chassis turn 90";
    command(turn, "OK chassis mode=2");
    char turn_set[] = "turn set kp 4";
    command(turn_set, "OK turn set kp=4");
    char turn_slider[] = "turn set kd:0.2";
    command(turn_slider, "OK turn set kd=0.2");
    char target_slider[] = "chassis turn:90";
    command(target_slider, "OK chassis mode=2");
    char hold_set[] = "hold set kd 0.1";
    command(hold_set, "OK hold set kd=0.1");
    char turn_brake_slider[] = "turn set brake:144";
    command(turn_brake_slider, "OK turn set brake=144");
    /* 制动减速度不能超过角加速度限制。 */
    {
        char turn_brake_big[] = "turn set brake 200";
        char response[1024];
        assert(chassis_commands_execute(turn_brake_big, response, sizeof(response)));
        assert(strstr(response, "ERR turn parameter or range") != NULL);
    }
    char hold_ki_slider[] = "hold set ki:0.1";
    command(hold_ki_slider, "OK hold set ki=0.1");
    char hold_izone[] = "hold set izone 5";
    command(hold_izone, "OK hold set izone=5");
    char hold_ilimit_slider[] = "hold set ilimit:5";
    command(hold_ilimit_slider, "OK hold set ilimit=5");
    char hold_get[] = "hold get";
    command(hold_get, "OK hold kp_m=1000 kd_m=100 ki_m=100");
    /* 转向环不开放积分参数。 */
    {
        char turn_ki[] = "turn set ki 0.1";
        char response[1024];
        assert(chassis_commands_execute(turn_ki, response, sizeof(response)));
        assert(strstr(response, "ERR turn parameter or range") != NULL);
    }
    char straight[] = "chassis straight 100 200 500";
    command(straight, "OK chassis mode=3");
    char stop[] = "chassis stop";
    command(stop, "OK chassis stop");
    char bad[] = "chassis vw 500 0 100";
    char response[1024];
    assert(chassis_commands_execute(bad, response, sizeof(response)));
    assert(strstr(response, "ERR") != NULL);
    assert(critical == 0U);
    puts("PASS: chassis command parsing, turn/straight and parameter set");
    return 0;
}
