#include "bsp/debug_uart.h"
#include "bsp/encoder.h"
#include "bsp/motor.h"
#include "config/wheel_pid_config.h"
#include "control/wheel_drive.h"
#include "debug/debug_console.h"
#include "debug/vofa_stream.h"
#include "runtime/runtime_monitor.h"

#include "cmsis_os2.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static int critical_depth;
static uint32_t now_ms;
static int32_t motor_output[WHEEL_COUNT];
static jmp_buf finished;
static char transcript[65536];
static size_t transcript_length;
static unsigned frame_count;
static double previous_time = -1.0;
static uint32_t stop_applied_ms;
static bool saw_nonzero_output;
static bool saw_updated_target;
static bool ran_after_stream_off;

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

/* 模拟普通用户按顺序操作；时间仅用于串口调度回归，不代表无线或电机性能。 */
static const struct
{
    uint32_t at_ms;
    const char *text;
} input[] = {
    {20, "vofa on\r\n"},
    {40, "pid edit left kp:542\n"},
    {60, "pid edit left ki:100\n"},
    {80, "pid edit left kd:0.001\n"},
    {100, "pid apply left\n"},
    {140, "pid get left\n"},
    {160, "wheel target left:1\n"},
    {180, "wheel start left\n"},
    {230, "wheel target left:2.500\n"},
    {260, "set report_ms 100\n"},
    {300, "vofa status\n"},
    {330, "pid edit left kp:bad\n"},
    {350, "wheel target left:bad\n"},
    {370, "help\n"},
    {450, "pid edit left kp:40\n"},
    {470, "pid apply left\n"},
    {600, "pid get left\n"},
    {2400, "vofa off\n"},
    {2600, "wheel target left:0.500\n"},
    {2700, "wheel stop\n"},
    {2800, "wheel target left:1.250\n"},
    {2900, "set report_ms 100\n"},
};
static size_t input_index;
static size_t input_byte;

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
    return true;
}

void motor_set_duty_permille(wheel_id_t wheel, int32_t duty)
{
    CHECK(critical_depth == 0);
    motor_output[wheel] = duty;
    if (wheel == WHEEL_RIGHT)
    {
        CHECK(duty == 0);
    }
    if (wheel == WHEEL_LEFT && duty != 0)
    {
        CHECK(now_ms >= 180 && now_ms <= 2720);
        saw_nonzero_output = true;
    }
}

bool encoder_init(void)
{
    return true;
}

int32_t encoder_read_delta_counts(wheel_id_t wheel, uint32_t *raw)
{
    (void)wheel;
    *raw = 0;
    return 0;
}

static void advance(uint32_t duration_ms)
{
    CHECK(critical_depth == 0);
    for (uint32_t i = 0; i < duration_ms; i++)
    {
        now_ms++;
        if (now_ms % 10U == 0U)
        {
            wheel_drive_step(now_ms, false);
            vofa_stream_capture();
            wheel_drive_status_t status;
            wheel_drive_get_status(&status);
            if (status.last_stop_reason == WHEEL_STOP_COMMAND && stop_applied_ms == 0U)
            {
                stop_applied_ms = now_ms;
            }
            if (now_ms >= 2200U && now_ms < 2700U)
            {
                CHECK(status.mode == WHEEL_MODE_SPEED_LOOP && status.remaining_ms == 0U);
                CHECK(status.enabled[WHEEL_LEFT] && !status.enabled[WHEEL_RIGHT]);
            }
            if (now_ms == 2500U)
            {
                vofa_stream_status_t stream;
                vofa_stream_get_status(&stream);
                CHECK(!stream.enabled && motor_output[WHEEL_LEFT] != 0);
                ran_after_stream_off = true;
            }
        }
        if (now_ms == 3300U)
        {
            longjmp(finished, 1);
        }
    }
}

bool debug_uart_start(void)
{
    return true;
}

bool debug_uart_read(uint8_t *byte, bool *data_lost, uint32_t timeout_ms)
{
    if (input_index < sizeof(input) / sizeof(input[0]) && now_ms >= input[input_index].at_ms)
    {
        *byte = (uint8_t)input[input_index].text[input_byte++];
        *data_lost = false;
        if (input[input_index].text[input_byte] == '\0')
        {
            input_index++;
            input_byte = 0U;
        }
        return true;
    }
    advance(timeout_ms);
    return false;
}

bool debug_uart_write(const char *data, uint16_t length)
{
    CHECK(critical_depth == 0);
    CHECK(transcript_length + length < sizeof(transcript));
    memcpy(transcript + transcript_length, data, length);
    transcript_length += length;
    transcript[transcript_length] = '\0';
    if (strncmp(data, "vofa:", 5) == 0)
    {
        double t, target, measured, error, duty, kp, ki, kd;
        CHECK(sscanf(data, "vofa:%lf,%lf,%lf,%lf,%lf,%lf,%lf,%lf", &t, &target, &measured, &error,
                     &duty, &kp, &ki, &kd) == 8);
        CHECK(t > previous_time && error == target - measured && kd == 0);
        CHECK((kp == 0 && ki == 0) || ((kp == 542 || kp == 40) && ki == 100));
        if (target == 2.5)
        {
            saw_updated_target = true;
        }
        previous_time = t;
        frame_count++;
    }
    else
    {
        /* 官方 FireWater 冒号分支不检查数值转换成功，文本不可带冒号。 */
        CHECK(memchr(data, ':', length) == NULL);
        CHECK(strncmp(data, "OK ", 3) == 0 || strncmp(data, "ERR ", 4) == 0 ||
              strncmp(data, "READY ", 6) == 0);
    }
    /* 按 115200/8N1 估算线占用，模拟发送时高优先级控制任务继续运行。 */
    advance(((uint32_t)length * 10000U + 115199U) / 115200U);
    return true;
}

void debug_uart_get_status(debug_uart_status_t *status)
{
    *status = (debug_uart_status_t){0};
}

void runtime_monitor_get_status(runtime_monitor_status_t *status)
{
    *status = (runtime_monitor_status_t){.period_ticks = 10, .rtos_tick_hz = 1000};
}

uint32_t osKernelGetTickCount(void)
{
    return now_ms;
}

osThreadId_t osThreadGetId(void)
{
    return NULL;
}

uint32_t osThreadGetStackSpace(osThreadId_t thread)
{
    (void)thread;
    return 0;
}

void Error_Handler(void)
{
    CHECK(false);
}

int main(void)
{
    CHECK(wheel_drive_init(wheel_pid_defaults));
    wheel_drive_step(0, false);
    if (setjmp(finished) == 0)
    {
        debug_console_run();
    }
    CHECK(input_index == sizeof(input) / sizeof(input[0]));
    CHECK(saw_nonzero_output && motor_output[0] == 0 && motor_output[1] == 0);
    CHECK(stop_applied_ms >= 2700 && stop_applied_ms <= 2720);
    CHECK(saw_updated_target && ran_after_stream_off);
    wheel_drive_status_t wheel;
    wheel_drive_get_status(&wheel);
    CHECK(wheel.mode == WHEEL_MODE_STOP && wheel.target_milli_counts[WHEEL_LEFT] == 0);
    CHECK(wheel.configured_target_milli_counts[WHEEL_LEFT] == 1250);
    const char *queued = strstr(transcript, "OK pid left queued id=1");
    const char *applied = strstr(transcript, "OK pid left applied id=1");
    CHECK(queued != NULL && applied != NULL && queued < applied);
    CHECK(strstr(transcript, "ERR pid kd_unsupported") != NULL);
    CHECK(strstr(transcript, "active kp=542.000 ki=100.000 kd=0.000") != NULL);
    CHECK(strstr(transcript, "active kp=40.000 ki=100.000 kd=0.000") != NULL);
    CHECK(strstr(transcript, "ERR report_ms requires vofa off") != NULL);
    CHECK(strstr(transcript, "OK report_ms=100") != NULL);
    CHECK(strstr(transcript, "OK status ") != NULL);
    vofa_stream_status_t stream;
    vofa_stream_get_status(&stream);
    CHECK(!stream.enabled && stream.dropped == 0 && stream.tx_errors == 0);
    CHECK(frame_count >= 210 && frame_count <= 239);
    printf("PASS: console integration, %u checks, %u frames, stop at %lu ms\n", checks, frame_count,
           (unsigned long)stop_applied_ms);
    return 0;
}
