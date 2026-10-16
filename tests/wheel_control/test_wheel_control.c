#include "bsp/encoder.h"
#include "bsp/motor.h"
#include "config/wheel_pid_config.h"
#include "control/wheel_drive.h"
#include "debug/wheel_commands.h"
#include "debug/wheel_status.h"
#include "debug/pid_commands.h"
#include "debug/vofa_stream.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static unsigned cases;
static int critical_depth;
static uint32_t tick;
static int32_t feedback[WHEEL_COUNT];
static uint32_t raw[WHEEL_COUNT];
static int32_t output[WHEEL_COUNT];
static wheel_drive_status_t status;

/* 仅供主机契约测试，不进入固件，也不代表电机已整定参数。 */
static const pid_pi_params_t test_params[WHEEL_COUNT] = {
    {.kp = 80.0f, .ki = 400.0f, .tracking_gain_per_s = 10.0f},
    {.kp = 80.0f, .ki = 400.0f, .tracking_gain_per_s = 10.0f},
};

/* 不用 assert，保证优化构建也执行契约检查。 */
#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        checks++;                                                                                  \
        if (!(condition))                                                                          \
        {                                                                                          \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                           \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

void test_enter_critical(void)
{
    critical_depth++;
}

void test_exit_critical(void)
{
    CHECK(critical_depth > 0);
    critical_depth--;
}

bool motor_init(void)
{
    output[0] = output[1] = 0;
    return true;
}

void motor_set_duty_permille(wheel_id_t wheel, int32_t duty)
{
    CHECK(critical_depth == 0);
    CHECK(duty >= -300 && duty <= 300);
    output[wheel] = duty;
}

bool encoder_init(void)
{
    raw[0] = raw[1] = 0;
    return true;
}

int32_t encoder_read_delta_counts(wheel_id_t wheel, uint32_t *raw_count)
{
    CHECK(critical_depth == 0);
    raw[wheel] = (raw[wheel] + (uint32_t)feedback[wheel]) & 65535U;
    *raw_count = raw[wheel];
    return feedback[wheel];
}

static void sample(uint32_t interval, int32_t left, int32_t right, bool delayed)
{
    tick += interval;
    feedback[0] = left;
    feedback[1] = right;
    wheel_drive_step(tick, delayed);
    wheel_drive_get_status(&status);
    CHECK(critical_depth == 0);
    CHECK(status.duty_permille[0] == output[0] && status.duty_permille[1] == output[1]);
}

static void start_at(uint32_t initial_tick)
{
    tick = initial_tick;
    CHECK(wheel_drive_init(test_params));
    sample(0, 0, 0, false);
    CHECK(status.ready && status.mode == WHEEL_MODE_STOP && !status.period_fault);
}

static void start_case(void)
{
    cases++;
    start_at(0);
}

/* 场景初始化时同时配置目标并显式启动；单独调参另行直接调用设置接口。 */
static wheel_request_result_t request_speed(wheel_drive_selection_t selection, int32_t left,
                                            int32_t right)
{
    const wheel_request_result_t result = wheel_drive_set_speed_target(selection, left, right);
    return result == WHEEL_REQUEST_ACCEPTED ? wheel_drive_start_speed(selection) : result;
}

static void expect_stopped(wheel_stop_reason_t reason)
{
    CHECK(status.mode == WHEEL_MODE_STOP && status.remaining_ms == 0);
    CHECK(status.last_stop_reason == reason && !status.transitioning);
    for (wheel_id_t wheel = WHEEL_LEFT; wheel < WHEEL_COUNT; wheel++)
    {
        CHECK(!status.enabled[wheel] && output[wheel] == 0);
        CHECK(status.pi[wheel].output_permille == 0 && status.target_milli_counts[wheel] == 0);
        CHECK(status.pi[wheel].proportional_permille == 0);
        CHECK(status.pi[wheel].integral_permille == 0 &&
              status.pi[wheel].next_integral_permille == 0);
        CHECK(status.requested_duty_permille[wheel] == 0);
    }
}

static void boot_and_validation(void)
{
    cases++;
    CHECK(wheel_drive_init(test_params));
    CHECK(wheel_drive_request_open_loop(100, 0, 100) == WHEEL_REQUEST_NOT_READY);
    CHECK(request_speed(WHEEL_SELECT_LEFT, 1000, 0) == WHEEL_REQUEST_NOT_READY);
    CHECK(wheel_drive_start_speed(WHEEL_SELECT_LEFT) == WHEEL_REQUEST_NOT_READY);
    tick = 0;
    sample(0, 0, 0, false);
    expect_stopped(WHEEL_STOP_BOOT);
    CHECK(wheel_drive_request_open_loop(301, 0, 100) == WHEEL_REQUEST_INVALID);
    CHECK(wheel_drive_request_open_loop(0, -301, 100) == WHEEL_REQUEST_INVALID);
    const uint32_t invalid_durations[] = {0, 1, 9, 11, 1999, 2001, UINT32_MAX};
    for (unsigned i = 0; i < sizeof(invalid_durations) / sizeof(invalid_durations[0]); i++)
    {
        CHECK(wheel_drive_request_open_loop(100, 0, invalid_durations[i]) == WHEEL_REQUEST_INVALID);
    }
    CHECK(request_speed(WHEEL_SELECT_LEFT, 10001, 0) == WHEEL_REQUEST_INVALID);
    CHECK(request_speed(WHEEL_SELECT_RIGHT, 0, -10001) == WHEEL_REQUEST_INVALID);
    CHECK(request_speed(WHEEL_SELECT_LEFT, 0, 1) == WHEEL_REQUEST_INVALID);
    CHECK(request_speed((wheel_drive_selection_t)3, 0, 0) == WHEEL_REQUEST_INVALID);
    CHECK(wheel_drive_start_speed((wheel_drive_selection_t)3) == WHEEL_REQUEST_INVALID);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_BOOT);
    CHECK(status.last_reject_reason == WHEEL_REQUEST_INVALID);
}

static void open_loop_and_expiry(void)
{
    start_case();
    CHECK(wheel_drive_request_open_loop(300, -300, 20) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 1, -1, false);
    CHECK(status.mode == WHEEL_MODE_OPEN_LOOP && status.remaining_ms == 20);
    CHECK(output[0] == 300 && output[1] == -300);
    sample(10, 2, -2, false);
    CHECK(status.remaining_ms == 10 && status.total_counts[0] == 3);
    sample(10, 3, -3, false);
    expect_stopped(WHEEL_STOP_EXPIRED);
    CHECK(status.total_counts[0] == 6 && status.total_counts[1] == -6);
    sample(10, 1, -1, false);
    CHECK(status.total_counts[0] == 7 && output[0] == 0);
    CHECK(wheel_drive_request_open_loop(0, 0, 10) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.mode == WHEEL_MODE_OPEN_LOOP && output[0] == 0 && output[1] == 0);
}

static void zero_target_and_reversal(void)
{
    start_case();
    CHECK(request_speed(WHEEL_SELECT_LEFT, 1000, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 7, false);
    CHECK(status.pi[0].output_permille == 80 && status.requested_duty_permille[0] == 80);
    CHECK(output[0] == 80 && !status.limited[0]);
    CHECK(!status.enabled[1] && status.pi[1].output_permille == 0 && output[1] == 0);
    CHECK(wheel_drive_set_speed_target(WHEEL_SELECT_LEFT, 0, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.mode == WHEEL_MODE_SPEED_LOOP && status.pi[0].output_permille == 4);
    CHECK(output[0] == 4 && !status.limited[0]);
    sample(10, 2, 0, false);
    CHECK(status.pi[0].output_permille == -156 && status.reversing[0] && output[0] == 0);
    CHECK(fabsf(status.pi[0].next_integral_permille - 11.6f) < 0.001f);
    sample(10, 2, 0, false);
    CHECK(fabsf(status.pi[0].output_permille + 148.4f) < 0.001f);
    CHECK(!status.reversing[0] && output[0] == -148);
    wheel_drive_stop();
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
    CHECK(request_speed(WHEEL_SELECT_LEFT, 0, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 0 && status.mode == WHEEL_MODE_SPEED_LOOP);
}

static void independent_controllers(void)
{
    start_case();
    CHECK(request_speed(WHEEL_SELECT_BOTH, 250, -250) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 20 && status.pi[1].output_permille == -20);
    CHECK(output[0] == 20 && output[1] == -20);
    CHECK(request_speed(WHEEL_SELECT_LEFT, 250, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 8, false);
    CHECK(status.pi[0].output_permille == 21 && output[0] == 21);
    CHECK(!status.enabled[1] && status.pi[1].output_permille == 0 && output[1] == 0);
    CHECK(request_speed(WHEEL_SELECT_BOTH, 250, -250) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 22 && status.pi[1].output_permille == -20);
    CHECK(request_speed(WHEEL_SELECT_RIGHT, 0, -250) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 8, 0, false);
    CHECK(!status.enabled[0] && status.pi[0].output_permille == 0 && output[0] == 0);
    CHECK(status.pi[1].output_permille == -21);
}

static void transitions_and_stop_priority(void)
{
    start_case();
    CHECK(wheel_drive_request_open_loop(100, 100, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(request_speed(WHEEL_SELECT_BOTH, 250, 250) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.transitioning && status.remaining_ms == 0 && output[0] == 0 && output[1] == 0);
    CHECK(status.pi[0].output_permille == 0);
    sample(10, 0, 0, false);
    CHECK(!status.transitioning && status.remaining_ms == 0 && output[0] == 20);
    CHECK(wheel_drive_request_open_loop(-100, -100, 20) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.transitioning && output[0] == 0 && status.pi[0].output_permille == 0);
    sample(10, 0, 0, false);
    CHECK(!status.transitioning && output[0] == -100 && status.remaining_ms == 10);
    CHECK(request_speed(WHEEL_SELECT_BOTH, 250, 250) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.transitioning && output[0] == 0);
    sample(10, 0, 0, false);
    CHECK(status.mode == WHEEL_MODE_SPEED_LOOP && !status.transitioning && output[0] == 20);
    /* 最短开环请求包含过渡周期，到期后不额外补一个出力周期。 */
    CHECK(wheel_drive_request_open_loop(100, 0, 10) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.transitioning && output[0] == 0);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_EXPIRED);

    CHECK(wheel_drive_request_open_loop(100, 0, 100) == WHEEL_REQUEST_ACCEPTED);
    wheel_drive_stop();
    CHECK(request_speed(WHEEL_SELECT_LEFT, 1000, 0) == WHEEL_REQUEST_STOP_PENDING);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
    CHECK(request_speed(WHEEL_SELECT_LEFT, 250, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(output[0] == 20);
    CHECK(wheel_drive_request_open_loop(-100, 0, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.transitioning);
    wheel_drive_stop();
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
}

static void repeated_start_and_wrap(void)
{
    start_case();
    CHECK(request_speed(WHEEL_SELECT_LEFT, 250, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 20);
    CHECK(request_speed(WHEEL_SELECT_LEFT, 250, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 21 && status.remaining_ms == 0);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 22 && status.mode == WHEEL_MODE_SPEED_LOOP);
    wheel_drive_stop();
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
    CHECK(request_speed(WHEEL_SELECT_LEFT, 250, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 20);
    start_at(UINT32_MAX - 15U);
    CHECK(request_speed(WHEEL_SELECT_LEFT, 250, 0) == WHEEL_REQUEST_ACCEPTED);
    for (unsigned i = 0; i < 250; i++)
    {
        sample(10, 0, 0, false);
        CHECK(status.mode == WHEEL_MODE_SPEED_LOOP && !status.period_fault);
        CHECK(status.remaining_ms == 0 && status.enabled[0] && !status.enabled[1]);
    }
    start_at(UINT32_MAX - 15U);
    CHECK(wheel_drive_request_open_loop(100, 0, 20) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    sample(10, 0, 0, false);
    CHECK(!status.period_fault && status.remaining_ms == 10 && output[0] == 100);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_EXPIRED);
}

static void independent_target_and_continuous_run(void)
{
    start_case();
    CHECK(wheel_drive_set_speed_target(WHEEL_SELECT_BOTH, 250, -500) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_BOOT);
    CHECK(status.configured_target_milli_counts[0] == 250);
    CHECK(status.configured_target_milli_counts[1] == -500);
    CHECK(wheel_drive_start_speed(WHEEL_SELECT_LEFT) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 20 && status.pi[0].next_integral_permille == 1);
    CHECK(wheel_drive_set_speed_target(WHEEL_SELECT_LEFT, 500, 0) == WHEEL_REQUEST_ACCEPTED);
    CHECK(wheel_drive_set_speed_target(WHEEL_SELECT_LEFT, 10001, 0) == WHEEL_REQUEST_INVALID);
    wheel_drive_get_status(&status);
    CHECK(status.target_milli_counts[0] == 250);
    sample(10, 0, 0, false);
    CHECK(status.target_milli_counts[0] == 500 && output[0] == 41);
    CHECK(status.pi[0].integral_permille == 1);
    CHECK(status.configured_target_milli_counts[1] == -500);
    CHECK(wheel_drive_set_speed_target(WHEEL_SELECT_RIGHT, 0, 1000) == WHEEL_REQUEST_ACCEPTED);
    for (unsigned i = 0; i < 350; i++)
    {
        sample(10, 0, 2, false);
        CHECK(status.mode == WHEEL_MODE_SPEED_LOOP && status.remaining_ms == 0);
        CHECK(status.enabled[0] && !status.enabled[1] && output[1] == 0);
        CHECK(status.target_milli_counts[0] == 500 && status.target_milli_counts[1] == 0);
    }
    CHECK(status.configured_target_milli_counts[1] == 1000);
    wheel_drive_stop();
    /* 停止与改目标同周期时只保存参数，不重新启用闭环。 */
    CHECK(wheel_drive_set_speed_target(WHEEL_SELECT_LEFT, 750, 0) == WHEEL_REQUEST_ACCEPTED);
    CHECK(wheel_drive_start_speed(WHEEL_SELECT_LEFT) == WHEEL_REQUEST_STOP_PENDING);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
    CHECK(status.configured_target_milli_counts[0] == 750);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
    CHECK(wheel_drive_start_speed(WHEEL_SELECT_LEFT) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(output[0] == 60 && status.target_milli_counts[0] == 750);
    CHECK(wheel_drive_request_open_loop(100, 0, 20) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.transitioning && status.remaining_ms == 20);
    CHECK(wheel_drive_set_speed_target(WHEEL_SELECT_LEFT, 1000, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.mode == WHEEL_MODE_OPEN_LOOP && output[0] == 100);
    CHECK(status.target_milli_counts[0] == 0 && status.remaining_ms == 10);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_EXPIRED);
    start_at(0);
    CHECK(status.configured_target_milli_counts[0] == 0 &&
          status.configured_target_milli_counts[1] == 0);
}

static void independent_reversal_and_pending_updates(void)
{
    start_case();
    CHECK(request_speed(WHEEL_SELECT_BOTH, 250, 250) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(request_speed(WHEEL_SELECT_BOTH, -250, 250) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == -19 && status.pi[1].output_permille == 21);
    CHECK(status.reversing[0] && !status.reversing[1] && output[0] == 0 && output[1] == 21);
    sample(10, 0, 0, false);
    CHECK(fabsf(status.pi[0].output_permille + 18.1f) < 0.001f);
    CHECK(output[0] == -18 && output[1] == 22);
    CHECK(status.remaining_ms == 0);
    CHECK(request_speed(WHEEL_SELECT_BOTH, 0, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.mode == WHEEL_MODE_SPEED_LOOP && status.enabled[0] && status.enabled[1]);
    CHECK(fabsf(status.pi[0].output_permille - 0.9f) < 0.001f);
    CHECK(status.pi[1].output_permille == 3);
    CHECK(output[0] == 0 && output[1] == 3);

    start_at(0);
    char response[1024];
    CHECK(wheel_commands_execute("wheel left 150 100", response, sizeof(response)));
    CHECK(wheel_commands_execute("wheel left 301 100", response, sizeof(response)));
    CHECK(strncmp(response, "ERR ", 4) == 0);
    sample(10, 0, 0, false);
    CHECK(output[0] == 150);
    CHECK(wheel_drive_request_open_loop(-150, 100, 100) == WHEEL_REQUEST_ACCEPTED);
    CHECK(wheel_drive_request_open_loop(-200, 200, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.reversing[0] && !status.reversing[1] && output[0] == 0 && output[1] == 200);
    sample(10, 0, 0, false);
    CHECK(output[0] == -200 && output[1] == 200 && status.remaining_ms == 90);
}

static void period_faults(void)
{
    const uint32_t intervals[] = {0, 9, 11, 20};
    for (unsigned i = 0; i < sizeof(intervals) / sizeof(intervals[0]); i++)
    {
        start_case();
        CHECK(request_speed(WHEEL_SELECT_BOTH, 250, 250) == WHEEL_REQUEST_ACCEPTED);
        sample(10, 0, 0, false);
        CHECK(wheel_drive_request_open_loop(100, 100, 100) == WHEEL_REQUEST_ACCEPTED);
        sample(intervals[i], 100, -100, false);
        expect_stopped(WHEEL_STOP_PERIOD_ERROR);
        CHECK(status.period_fault && status.total_counts[0] == 100);
        CHECK(wheel_drive_request_open_loop(100, 0, 100) == WHEEL_REQUEST_PERIOD_FAULT);
        CHECK(request_speed(WHEEL_SELECT_LEFT, 1000, 0) == WHEEL_REQUEST_PERIOD_FAULT);
        CHECK(wheel_drive_start_speed(WHEEL_SELECT_LEFT) == WHEEL_REQUEST_PERIOD_FAULT);
        wheel_drive_stop();
        sample(10, 1, 0, false);
        expect_stopped(WHEEL_STOP_PERIOD_ERROR);
        CHECK(status.period_fault && status.total_counts[0] == 101);
    }
    start_case();
    CHECK(wheel_drive_request_open_loop(100, 0, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, true);
    expect_stopped(WHEEL_STOP_PERIOD_ERROR);
}

static void command_parsing_and_status(void)
{
    start_case();
    char response[1024];
    CHECK(!wheel_commands_execute("ping", response, sizeof(response)));
    const char *invalid[] = {
        "wheel",
        "wheel speed",
        "wheel speed left 1 2000",
        "wheel target left nan",
        "wheel target left inf",
        "wheel target left 1e0",
        "wheel target left +1",
        "wheel target left .5",
        "wheel target left 1.",
        "wheel target left 0.0001",
        "wheel target left 10.001",
        "wheel target left -10.001",
        "wheel target both 1",
        "wheel target left 1 2000",
        "wheel target left:",
        "wheel target left::1",
        "wheel target left:10.001",
        "wheel target left=1",
        "wheel target left:1 extra",
        "wheel start left 1",
        "wheel start left:",
        "wheel start leftx",
        "wheel start",
        "wheel left 1.5 100",
        "wheel both 100 301 100",
        "wheel left 9999999999999999999 100",
    };
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
    {
        CHECK(wheel_commands_execute(invalid[i], response, sizeof(response)));
        CHECK(strncmp(response, "ERR ", 4) == 0);
        sample(10, 0, 0, false);
        expect_stopped(WHEEL_STOP_BOOT);
    }
    CHECK(wheel_commands_execute("wheel target both 0.250 -0.25", response, sizeof(response)));
    CHECK(strcmp(response, "OK wheel command queued\r\n") == 0);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_BOOT);
    CHECK(wheel_commands_execute("wheel start both", response, sizeof(response)));
    sample(10, 0, 0, false);
    CHECK(status.target_milli_counts[0] == 250 && status.target_milli_counts[1] == -250);
    CHECK(output[0] == 20 && output[1] == -20);
    CHECK(wheel_commands_execute("wheel status", response, sizeof(response)));
    CHECK(strstr(response, "mode=speed source=debug") != NULL);
    CHECK(strstr(response, "controller=pi output_unit=permille") != NULL);
    CHECK(strstr(response, "setpoint_left_milli=250 setpoint_right_milli=-250") != NULL);
    CHECK(strstr(response, "p_left=20.000 p_right=-20.000 i_left=0.000 i_right=0.000") != NULL);
    CHECK(strstr(response, "i_next_left=1.000 i_next_right=-1.000") != NULL);
    CHECK(strstr(response, "pi_left=20.000 pi_right=-20.000") != NULL);
    CHECK(strstr(response, "duty_left=20 duty_right=-20") != NULL);
    CHECK(strlen(response) < 1024 && strstr(response, "raw_right=0\r\n") != NULL);
    CHECK(wheel_commands_execute("wheel stop", response, sizeof(response)));
    CHECK(wheel_commands_execute("wheel left 150 100", response, sizeof(response)));
    CHECK(strstr(response, "stop_pending") != NULL);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
    wheel_status_format(response, 40);
    CHECK(strcmp(response, "ERR wheel status buffer\r\n") == 0);
    CHECK(wheel_commands_execute("wheel target right:-10.000", response, sizeof(response)));
    CHECK(wheel_commands_execute("wheel start right", response, sizeof(response)));
    sample(10, 0, 0, false);
    CHECK(status.enabled[1] && !status.enabled[0] && output[1] == -250);
    sample(11, 0, 0, false);
    CHECK(wheel_commands_execute("wheel target left:1", response, sizeof(response)));
    CHECK(strstr(response, "period_fault_reset_required") != NULL);

    start_at(UINT32_MAX - 30U);
    CHECK(request_speed(WHEEL_SELECT_BOTH, -10000, 10000) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 32767, -32768, false);
    wheel_status_format(response, sizeof(response));
    CHECK(strncmp(response, "OK wheel ", 9) == 0 && strstr(response, "\r\n") != NULL);
    CHECK(strlen(response) < 1024);
    printf("Diagnostic stress response: %u bytes\n", (unsigned)strlen(response));
}

static void saturation_and_recovery(void)
{
    start_case();
    CHECK(request_speed(WHEEL_SELECT_BOTH, 10000, -10000) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 800 && output[0] == 250);
    CHECK(status.pi[1].output_permille == -800 && output[1] == -250);
    CHECK(status.pi[0].next_integral_permille == -15);
    CHECK(status.pi[1].next_integral_permille == 15);
    for (unsigned i = 0; i < 150; i++)
    {
        sample(10, 0, 0, false);
        CHECK(status.limited[0] && status.limited[1]);
        CHECK(status.requested_duty_permille[0] == 250 && output[0] == 250);
        CHECK(status.requested_duty_permille[1] == -250 && output[1] == -250);
        CHECK(status.pi[0].next_integral_permille >= -150.01f);
        CHECK(status.pi[1].next_integral_permille <= 150.01f);
    }
    CHECK(fabsf(status.pi[0].next_integral_permille + 150) < 0.01f);
    CHECK(fabsf(status.pi[1].next_integral_permille - 150) < 0.01f);
    CHECK(request_speed(WHEEL_SELECT_BOTH, 3000, -3000) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(!status.limited[0] && !status.limited[1]);
    CHECK(output[0] >= 89 && output[0] <= 90 && output[1] == -output[0]);
    wheel_drive_stop();
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
}

static void defaults_and_parameter_isolation(void)
{
    cases++;
    tick = 0;
    CHECK(wheel_drive_init(wheel_pid_defaults));
    sample(0, 0, 0, false);
    expect_stopped(WHEEL_STOP_BOOT);
    CHECK(request_speed(WHEEL_SELECT_BOTH, 10000, -10000) == WHEEL_REQUEST_ACCEPTED);
    for (unsigned i = 0; i < 5; i++)
    {
        sample(10, 2, -3, false);
        CHECK(status.mode == WHEEL_MODE_SPEED_LOOP && output[0] == 0 && output[1] == 0);
        CHECK(status.pi[0].next_integral_permille == 0 && status.pi[1].next_integral_permille == 0);
    }
    wheel_drive_stop();
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);

    /* 参数在初始化时复制；左轮配置不会改动右轮，也不借用调用方的可变内存。 */
    pid_pi_params_t params[WHEEL_COUNT] = {wheel_pid_defaults[0], wheel_pid_defaults[1]};
    params[0].kp = 40;
    tick = 0;
    CHECK(wheel_drive_init(params));
    params[0].kp = 999;
    sample(0, 0, 0, false);
    CHECK(request_speed(WHEEL_SELECT_BOTH, 1000, 1000) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(output[0] == 40 && output[1] == 0);
    sample(10, 2, 2, false);
    CHECK(status.reversing[0] && output[0] == 0 && output[1] == 0);
    CHECK(status.pi[0].next_integral_permille == 0);
    sample(10, 2, 2, false);
    CHECK(output[0] == -40 && output[1] == 0);
}

static void online_parameters_and_readback(void)
{
    start_case();
    pid_commands_init();
    char response[256];
    CHECK(!pid_commands_execute("ping", response, sizeof(response)));
    CHECK(pid_commands_execute("pid get left", response, sizeof(response)));
    CHECK(strstr(response, "active kp=80.000 ki=400.000 kd=0.000 d_enabled=0") != NULL);
    CHECK(pid_commands_execute("pid edit left kp 40.125", response, sizeof(response)));
    CHECK(strstr(response, "draft kp=40.125") != NULL);
    CHECK(pid_commands_execute("pid edit left ki 200", response, sizeof(response)));
    CHECK(pid_commands_execute("pid edit left kd 0", response, sizeof(response)));
    wheel_drive_get_status(&status);
    CHECK(status.pi[0].params.kp == 80 && status.pi[0].params.ki == 400);
    CHECK(pid_commands_execute("pid apply left", response, sizeof(response)));
    CHECK(strstr(response, "queued id=1") != NULL);
    CHECK(!pid_commands_poll(response, sizeof(response)));
    CHECK(pid_commands_execute("pid apply left", response, sizeof(response)));
    CHECK(strstr(response, "busy") != NULL);
    uint32_t id;
    CHECK(wheel_drive_request_left_pi(1000, 1000, &id) == WHEEL_REQUEST_BUSY);
    CHECK(pid_commands_execute("pid get left", response, sizeof(response)));
    CHECK(strstr(response, "active kp=80.000 ki=400.000") != NULL);
    CHECK(strstr(response, "pending=1") != NULL);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_BOOT);
    CHECK(status.pi[0].params.kp == 40.125f && status.pi[0].params.ki == 200);
    CHECK(status.pi[1].params.kp == 80 && status.pi[1].params.ki == 400);
    sample(10, 0, 0, false);
    CHECK(pid_commands_poll(response, sizeof(response)));
    CHECK(strstr(response, "applied id=1 tick_ms=10 kp=40.125 ki=200.000") != NULL);
    CHECK(!pid_commands_poll(response, sizeof(response)));
    CHECK(wheel_drive_request_left_pi(10000001, 0, &id) == WHEEL_REQUEST_INVALID);
    const char *invalid[] = {
        "pid edit left kd 0.001",
        "pid edit right kp 1",
        "pid apply right",
        "pid get right",
        "pid edit left kp nan",
        "pid edit left kp inf",
        "pid edit left kp 1e1",
        "pid edit left kp -1",
        "pid edit left kp .5",
        "pid edit left kp 1.",
        "pid edit left kp 1.0001",
        "pid edit left kp 10000.001",
        "pid edit left kp 9999999999999999999999999",
        "pid edit left kp 1 extra",
        "pid edit left kp",
        "pid",
        "pid edit left kd -0.001",
    };
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
    {
        CHECK(pid_commands_execute(invalid[i], response, sizeof(response)));
        CHECK(strncmp(response, "ERR ", 4) == 0);
    }
    CHECK(pid_commands_execute("pid apply left", response, sizeof(response)));
    sample(10, 0, 0, false);
    CHECK(pid_commands_poll(response, sizeof(response)));
    CHECK(status.pi[0].params.kp == 40.125f && status.pi[0].params.ki == 200);
    CHECK(pid_commands_execute("pid edit left kp 10000.000", response, sizeof(response)));
    CHECK(pid_commands_execute("pid edit left ki 0.001", response, sizeof(response)));
    CHECK(pid_commands_execute("pid apply left", response, sizeof(response)));
    sample(10, 0, 0, false);
    CHECK(status.pi[0].params.kp == 10000 && status.pi[0].params.ki == 0.001f);
}

static void slider_parameter_commands(void)
{
    start_case();
    pid_commands_init();
    char response[256];
    CHECK(pid_commands_execute("pid edit left kp:542", response, sizeof(response)));
    CHECK(strstr(response, "draft kp=542.000") != NULL);
    CHECK(strchr(response, ':') == NULL);
    CHECK(pid_commands_execute("pid edit left ki:100.125", response, sizeof(response)));
    CHECK(pid_commands_execute("pid edit left kd:0", response, sizeof(response)));
    wheel_drive_get_status(&status);
    CHECK(status.pi[0].params.kp == 80 && status.pi[0].params.ki == 400);
    CHECK(pid_commands_execute("pid apply left", response, sizeof(response)));
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_BOOT);
    CHECK(status.pi[0].params.kp == 542 && status.pi[0].params.ki == 100.125f);
    CHECK(status.pi[1].params.kp == 80 && status.pi[1].params.ki == 400);
    CHECK(pid_commands_poll(response, sizeof(response)));
    CHECK(strstr(response, "kp=542.000 ki=100.125 kd=0.000") != NULL);
    const char *invalid[] = {"pid edit left kp:",          "pid edit left kp::542",
                             "pid edit left kp=542",       "pid edit left kpx:542",
                             "pid edit left kp:-1",        "pid edit left kp:10000.001",
                             "pid edit left kp:1.0001",    "pid edit left ki:nan",
                             "pid edit left kp:542 extra", "pid edit left kd:1"};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++)
    {
        CHECK(pid_commands_execute(invalid[i], response, sizeof(response)));
        CHECK(strncmp(response, "ERR ", 4) == 0 && strchr(response, ':') == NULL);
    }
    CHECK(pid_commands_execute("pid apply left", response, sizeof(response)));
    sample(10, 0, 0, false);
    CHECK(status.pi[0].params.kp == 542 && status.pi[0].params.ki == 100.125f);
}

static void running_parameter_updates(void)
{
    start_case();
    uint32_t id;
    CHECK(request_speed(WHEEL_SELECT_LEFT, 1000, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].next_integral_permille == 4);
    CHECK(wheel_drive_request_left_pi(40000, 200000, &id) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(output[0] == 44 && status.pi[0].integral_permille == 4);
    CHECK(status.pi[0].next_integral_permille == 6 && status.remaining_ms == 0);
    CHECK(output[1] == 0 && !status.enabled[1]);
    CHECK(wheel_drive_request_left_pi(40000, 0, &id) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(output[0] == 40 && status.pi[0].integral_permille == 0);
    CHECK(status.pi[0].next_integral_permille == 0 && status.remaining_ms == 0);
    CHECK(wheel_drive_request_left_pi(0, 100000, &id) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(output[0] == 0 && status.pi[0].next_integral_permille == 1);
    sample(10, 0, 0, false);
    CHECK(output[0] == 1);
    CHECK(wheel_drive_request_left_pi(60000, 200000, &id) == WHEEL_REQUEST_ACCEPTED);
    wheel_drive_stop();
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
    CHECK(status.pi[0].params.kp == 60 && status.left_pi_update_result == WHEEL_PI_UPDATE_APPLIED);
    CHECK(request_speed(WHEEL_SELECT_LEFT, 1000, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(output[0] == 60);
    CHECK(wheel_drive_request_left_pi(20000, 100000, &id) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.mode == WHEEL_MODE_SPEED_LOOP && output[0] == 22);
    CHECK(wheel_drive_request_open_loop(100, 0, 10) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.transitioning);
    CHECK(wheel_drive_request_left_pi(25000, 100000, &id) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_EXPIRED);
    CHECK(status.pi[0].params.kp == 25);

    pid_commands_init();
    char response[256];
    CHECK(pid_commands_execute("pid edit left kp 30", response, sizeof(response)));
    CHECK(pid_commands_execute("pid apply left", response, sizeof(response)));
    sample(11, 0, 0, false);
    expect_stopped(WHEEL_STOP_PERIOD_ERROR);
    CHECK(status.pi[0].params.kp == 25);
    CHECK(pid_commands_poll(response, sizeof(response)));
    CHECK(strstr(response, "rejected") != NULL);
    CHECK(pid_commands_execute("pid apply left", response, sizeof(response)));
    CHECK(strstr(response, "ERR pid period_fault") != NULL);
}

static void vofa_samples_and_capacity(void)
{
    start_case();
    vofa_stream_stop();
    vofa_stream_capture();
    vofa_stream_status_t stream;
    char line[VOFA_LINE_CAPACITY];
    CHECK(vofa_stream_format_next(line, sizeof(line)) == 0);
    vofa_stream_start(tick);
    CHECK(request_speed(WHEEL_SELECT_LEFT, 1250, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 1, 0, false);
    vofa_stream_capture();
    uint32_t id;
    CHECK(wheel_drive_request_left_pi(40000, 200000, &id) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 1, 0, false);
    vofa_stream_capture();
    size_t length = vofa_stream_format_next(line, sizeof(line));
    CHECK(length == strlen(line));
    CHECK(strcmp(line, "vofa:0.010,1.250,1,0.250,20,80.000,400.000,0.000\r\n") == 0);
    vofa_stream_record_tx(true);
    length = vofa_stream_format_next(line, sizeof(line));
    CHECK(length == strlen(line));
    CHECK(strcmp(line, "vofa:0.020,1.250,1,0.250,11,40.000,200.000,0.000\r\n") == 0);
    vofa_stream_record_tx(false);
    vofa_stream_start(999U);
    vofa_stream_get_status(&stream);
    CHECK(stream.captured == 2 && stream.sent == 1 && stream.tx_errors == 1);
    CHECK(stream.queued == 0 && stream.dropped == 0);
    for (unsigned i = 0; i < VOFA_SAMPLE_CAPACITY + 3U; i++)
    {
        sample(10, 0, 0, false);
        vofa_stream_capture();
    }
    vofa_stream_get_status(&stream);
    CHECK(stream.queued == VOFA_SAMPLE_CAPACITY && stream.dropped == 3);
    CHECK(vofa_stream_format_next(line, sizeof(line)) > 0);
    CHECK(strncmp(line, "vofa:0.060,", 11) == 0);
    CHECK(vofa_stream_format_next(line, 8) == 0);
    vofa_stream_get_status(&stream);
    CHECK(stream.dropped == 4);
    vofa_stream_stop();
    CHECK(vofa_stream_format_next(line, sizeof(line)) == 0);
    wheel_drive_get_status(&status);
    CHECK(status.mode == WHEEL_MODE_SPEED_LOOP);
    vofa_stream_start(tick);
    vofa_stream_get_status(&stream);
    CHECK(stream.captured == 0 && stream.dropped == 0 && stream.tx_errors == 0);
    wheel_drive_stop();
    sample(10, -2, 0, false);
    vofa_stream_capture();
    CHECK(vofa_stream_format_next(line, sizeof(line)) > 0);
    CHECK(strstr(line, ",0.000,-2,2.000,0,") != NULL);
    vofa_stream_stop();
}

static void vofa_timestamp_and_line_bound(void)
{
    cases++;
    start_at(UINT32_MAX - 5U);
    vofa_stream_start(tick);
    sample(10, 0, 0, false);
    vofa_stream_capture();
    char line[VOFA_LINE_CAPACITY];
    CHECK(vofa_stream_format_next(line, sizeof(line)) > 0);
    CHECK(strncmp(line, "vofa:0.010,", 11) == 0);
    vofa_stream_stop();

    /* 人工跳到时间字段的最大宽度，核对全部合法字段下的线速上界。 */
    start_at(UINT32_MAX - 10U);
    vofa_stream_start(0U);
    uint32_t id;
    CHECK(wheel_drive_request_left_pi(10000000, 10000000, &id) == WHEEL_REQUEST_ACCEPTED);
    CHECK(request_speed(WHEEL_SELECT_LEFT, -10000, 0) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 32767, 0, false);
    vofa_stream_capture();
    const size_t length = vofa_stream_format_next(line, sizeof(line));
    CHECK(length == strlen(line) && length <= 76);
    CHECK(strncmp(line, "vofa:4294967.295,", 16) == 0);
    CHECK(strstr(line, ",-32777.000,-250,10000.000,10000.000,0.000\r\n") != NULL);
    printf("VOFA maximum-width frame: %u bytes\n", (unsigned)length);
    vofa_stream_stop();
}

int main(void)
{
    boot_and_validation();
    open_loop_and_expiry();
    zero_target_and_reversal();
    independent_controllers();
    transitions_and_stop_priority();
    repeated_start_and_wrap();
    independent_target_and_continuous_run();
    independent_reversal_and_pending_updates();
    period_faults();
    command_parsing_and_status();
    saturation_and_recovery();
    defaults_and_parameter_isolation();
    online_parameters_and_readback();
    slider_parameter_commands();
    running_parameter_updates();
    vofa_samples_and_capacity();
    vofa_timestamp_and_line_bound();
    printf("PASS: %u scenarios, %u checks\n", cases, checks);
    return 0;
}
