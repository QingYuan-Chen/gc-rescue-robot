#include "bsp/encoder.h"
#include "bsp/motor.h"
#include "config/wheel_pid_config.h"
#include "control/wheel_drive.h"
#include "debug/wheel_commands.h"
#include "debug/wheel_status.h"

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
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 1000, 0, 100) == WHEEL_REQUEST_NOT_READY);
    tick = 0;
    sample(0, 0, 0, false);
    expect_stopped(WHEEL_STOP_BOOT);
    CHECK(wheel_drive_request_open_loop(301, 0, 100) == WHEEL_REQUEST_INVALID);
    CHECK(wheel_drive_request_open_loop(0, -301, 100) == WHEEL_REQUEST_INVALID);
    const uint32_t invalid_durations[] = {0, 1, 9, 11, 1999, 2001, UINT32_MAX};
    for (unsigned i = 0; i < sizeof(invalid_durations) / sizeof(invalid_durations[0]); i++)
    {
        CHECK(wheel_drive_request_open_loop(100, 0, invalid_durations[i]) == WHEEL_REQUEST_INVALID);
        CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 0, 0, invalid_durations[i]) ==
              WHEEL_REQUEST_INVALID);
    }
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 10001, 0, 100) == WHEEL_REQUEST_INVALID);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_RIGHT, 0, -10001, 100) == WHEEL_REQUEST_INVALID);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 0, 1, 100) == WHEEL_REQUEST_INVALID);
    CHECK(wheel_drive_request_speed((wheel_drive_selection_t)3, 0, 0, 100) ==
          WHEEL_REQUEST_INVALID);
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
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 1000, 0, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 7, false);
    CHECK(status.pi[0].output_permille == 80 && status.requested_duty_permille[0] == 80);
    CHECK(output[0] == 80 && !status.limited[0]);
    CHECK(!status.enabled[1] && status.pi[1].output_permille == 0 && output[1] == 0);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 0, 0, 100) == WHEEL_REQUEST_ACCEPTED);
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
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 0, 0, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 0 && status.mode == WHEEL_MODE_SPEED_LOOP);
}

static void independent_controllers(void)
{
    start_case();
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 250, -250, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 20 && status.pi[1].output_permille == -20);
    CHECK(output[0] == 20 && output[1] == -20);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 250, 0, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 8, false);
    CHECK(status.pi[0].output_permille == 21 && output[0] == 21);
    CHECK(!status.enabled[1] && status.pi[1].output_permille == 0 && output[1] == 0);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 250, -250, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 22 && status.pi[1].output_permille == -20);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_RIGHT, 0, -250, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 8, 0, false);
    CHECK(!status.enabled[0] && status.pi[0].output_permille == 0 && output[0] == 0);
    CHECK(status.pi[1].output_permille == -21);
}

static void transitions_and_stop_priority(void)
{
    start_case();
    CHECK(wheel_drive_request_open_loop(100, 100, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 250, 250, 30) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.transitioning && status.remaining_ms == 30 && output[0] == 0 && output[1] == 0);
    CHECK(status.pi[0].output_permille == 0);
    sample(10, 0, 0, false);
    CHECK(!status.transitioning && status.remaining_ms == 20 && output[0] == 20);
    CHECK(wheel_drive_request_open_loop(-100, -100, 20) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.transitioning && output[0] == 0 && status.pi[0].output_permille == 0);
    sample(10, 0, 0, false);
    CHECK(!status.transitioning && output[0] == -100 && status.remaining_ms == 10);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 250, 250, 10) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.transitioning && output[0] == 0);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_EXPIRED);

    CHECK(wheel_drive_request_open_loop(100, 0, 100) == WHEEL_REQUEST_ACCEPTED);
    wheel_drive_stop();
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 1000, 0, 100) == WHEEL_REQUEST_STOP_PENDING);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_COMMAND);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 250, 0, 100) == WHEEL_REQUEST_ACCEPTED);
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

static void renewal_and_wrap(void)
{
    start_case();
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 250, 0, 10) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 20);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 250, 0, 10) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 21 && status.remaining_ms == 10);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_EXPIRED);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 250, 0, 10) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == 20);
    start_at(UINT32_MAX - 15U);
    CHECK(wheel_drive_request_open_loop(100, 0, 20) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    sample(10, 0, 0, false);
    CHECK(!status.period_fault && status.remaining_ms == 10 && output[0] == 100);
    sample(10, 0, 0, false);
    expect_stopped(WHEEL_STOP_EXPIRED);
}

static void independent_reversal_and_pending_updates(void)
{
    start_case();
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 250, 250, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, -250, 250, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(status.pi[0].output_permille == -19 && status.pi[1].output_permille == 21);
    CHECK(status.reversing[0] && !status.reversing[1] && output[0] == 0 && output[1] == 21);
    sample(10, 0, 0, false);
    CHECK(fabsf(status.pi[0].output_permille + 18.1f) < 0.001f);
    CHECK(output[0] == -18 && output[1] == 22);
    CHECK(status.remaining_ms == 90);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 0, 0, 100) == WHEEL_REQUEST_ACCEPTED);
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
        CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 250, 250, 100) ==
              WHEEL_REQUEST_ACCEPTED);
        sample(10, 0, 0, false);
        CHECK(wheel_drive_request_open_loop(100, 100, 100) == WHEEL_REQUEST_ACCEPTED);
        sample(intervals[i], 100, -100, false);
        expect_stopped(WHEEL_STOP_PERIOD_ERROR);
        CHECK(status.period_fault && status.total_counts[0] == 100);
        CHECK(wheel_drive_request_open_loop(100, 0, 100) == WHEEL_REQUEST_PERIOD_FAULT);
        CHECK(wheel_drive_request_speed(WHEEL_SELECT_LEFT, 1000, 0, 100) ==
              WHEEL_REQUEST_PERIOD_FAULT);
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
        "wheel speed left nan 100",
        "wheel speed left inf 100",
        "wheel speed left 1e0 100",
        "wheel speed left +1 100",
        "wheel speed left .5 100",
        "wheel speed left 1. 100",
        "wheel speed left 0.0001 100",
        "wheel speed left 10.001 100",
        "wheel speed left -10.001 100",
        "wheel speed both 1 100",
        "wheel speed left 1 11",
        "wheel speed left 1 2001",
        "wheel speed left 1 100 extra",
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
    CHECK(wheel_commands_execute("wheel speed both 0.250 -0.25 100", response, sizeof(response)));
    CHECK(strcmp(response, "OK wheel command queued\r\n") == 0);
    sample(10, 0, 0, false);
    CHECK(status.target_milli_counts[0] == 250 && status.target_milli_counts[1] == -250);
    CHECK(output[0] == 20 && output[1] == -20);
    CHECK(wheel_commands_execute("wheel status", response, sizeof(response)));
    CHECK(strstr(response, "mode=speed source=debug") != NULL);
    CHECK(strstr(response, "controller=pi output_unit=permille") != NULL);
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
    CHECK(wheel_commands_execute("wheel speed right -10.000 2000", response, sizeof(response)));
    sample(10, 0, 0, false);
    CHECK(status.enabled[1] && !status.enabled[0] && output[1] == -250);
    sample(11, 0, 0, false);
    CHECK(wheel_commands_execute("wheel speed left 1 100", response, sizeof(response)));
    CHECK(strstr(response, "period_fault_reset_required") != NULL);

    start_at(UINT32_MAX - 30U);
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, -10000, 10000, 2000) ==
          WHEEL_REQUEST_ACCEPTED);
    sample(10, 32767, -32768, false);
    wheel_status_format(response, sizeof(response));
    CHECK(strncmp(response, "OK wheel ", 9) == 0 && strstr(response, "\r\n") != NULL);
    CHECK(strlen(response) < 1024);
    printf("Diagnostic stress response: %u bytes\n", (unsigned)strlen(response));
}

static void saturation_and_recovery(void)
{
    start_case();
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 10000, -10000, 2000) ==
          WHEEL_REQUEST_ACCEPTED);
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
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 3000, -3000, 100) == WHEEL_REQUEST_ACCEPTED);
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
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 10000, -10000, 100) ==
          WHEEL_REQUEST_ACCEPTED);
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
    CHECK(wheel_drive_request_speed(WHEEL_SELECT_BOTH, 1000, 1000, 100) == WHEEL_REQUEST_ACCEPTED);
    sample(10, 0, 0, false);
    CHECK(output[0] == 40 && output[1] == 0);
    sample(10, 2, 2, false);
    CHECK(status.reversing[0] && output[0] == 0 && output[1] == 0);
    CHECK(status.pi[0].next_integral_permille == 0);
    sample(10, 2, 2, false);
    CHECK(output[0] == -40 && output[1] == 0);
}

int main(void)
{
    boot_and_validation();
    open_loop_and_expiry();
    zero_target_and_reversal();
    independent_controllers();
    transitions_and_stop_priority();
    renewal_and_wrap();
    independent_reversal_and_pending_updates();
    period_faults();
    command_parsing_and_status();
    saturation_and_recovery();
    defaults_and_parameter_isolation();
    printf("PASS: %u scenarios, %u checks\n", cases, checks);
    return 0;
}
