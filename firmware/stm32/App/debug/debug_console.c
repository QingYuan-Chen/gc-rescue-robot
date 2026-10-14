#include "debug_console.h"
#include "wheel_commands.h"
#include "wheel_status.h"

#include "bsp/debug_uart.h"
#include "runtime/runtime_monitor.h"

#include "cmsis_os2.h"
#include "main.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_CONSOLE_LINE_CAPACITY 96U
#define DEBUG_CONSOLE_REPORT_MIN_MS 100U
#define DEBUG_CONSOLE_REPORT_MAX_MS 60000U

/* 当前仅调试任务访问；参数只保存在 RAM，复位后关闭自动上报。 */
static uint32_t report_ms;
static uint32_t last_report_tick;
/* 仅由调试任务复用，避免扩展状态字符串占用调用链上的任务栈。 */
static char response[1024];

static void write_text(const char *text)
{
    debug_uart_write(text, (uint16_t)strlen(text));
}

static void report_status(void)
{
    runtime_monitor_status_t runtime;
    debug_uart_status_t uart;
    runtime_monitor_get_status(&runtime);
    debug_uart_get_status(&uart);

    const int length =
        snprintf(response, sizeof(response),
                 "OK status clock_hz=%lu tick_hz=%lu period_ticks=%lu samples=%lu "
                 "hal_ms=%lu rtos_tick=%lu interval=%lu min=%lu max=%lu delay_errors=%lu "
                 "robot_stack_free=%lu debug_stack_free=%lu report_ms=%lu rx_bytes=%lu "
                 "tx_bytes=%lu rx_overflow=%lu rx_errors=%lu tx_errors=%lu\r\n",
                 (unsigned long)runtime.system_clock_hz, (unsigned long)runtime.rtos_tick_hz,
                 (unsigned long)runtime.period_ticks, (unsigned long)runtime.sample_count,
                 (unsigned long)runtime.hal_tick_ms, (unsigned long)runtime.rtos_tick,
                 (unsigned long)runtime.interval_ticks, (unsigned long)runtime.min_interval_ticks,
                 (unsigned long)runtime.max_interval_ticks,
                 (unsigned long)runtime.delay_error_count, (unsigned long)runtime.stack_free_bytes,
                 (unsigned long)osThreadGetStackSpace(osThreadGetId()), (unsigned long)report_ms,
                 (unsigned long)uart.rx_bytes, (unsigned long)uart.tx_bytes,
                 (unsigned long)uart.rx_overflow_count, (unsigned long)uart.rx_error_count,
                 (unsigned long)uart.tx_error_count);
    if (length > 0 && (size_t)length < sizeof(response))
    {
        debug_uart_write(response, (uint16_t)length);
    }
    wheel_status_format(response, sizeof(response));
    write_text(response);
}

static void report_parameter(void)
{
    char response[48];
    snprintf(response, sizeof(response), "OK report_ms=%lu\r\n", (unsigned long)report_ms);
    write_text(response);
}

static bool parse_report_ms(const char *text, uint32_t *value)
{
    uint32_t parsed = 0U;
    if (*text == '\0')
    {
        return false;
    }
    for (; *text != '\0'; text++)
    {
        if (*text < '0' || *text > '9')
        {
            return false;
        }
        parsed = parsed * 10U + (uint32_t)(*text - '0');
        if (parsed > DEBUG_CONSOLE_REPORT_MAX_MS)
        {
            return false;
        }
    }
    if (parsed != 0U && parsed < DEBUG_CONSOLE_REPORT_MIN_MS)
    {
        return false;
    }
    *value = parsed;
    return true;
}

static void execute_line(char *line)
{
    while (*line == ' ' || *line == '\t')
    {
        line++;
    }
    size_t length = strlen(line);
    while (length > 0U && (line[length - 1U] == ' ' || line[length - 1U] == '\t'))
    {
        line[--length] = '\0';
    }

    if (length == 0U)
    {
        return;
    }
    if (wheel_commands_execute(line, response, sizeof(response)))
    {
        write_text(response);
        return;
    }
    if (strcmp(line, "help") == 0)
    {
        write_text("OK help: ping | status | get report_ms | set report_ms <0|100..60000> | "
                   "wheel status/stop | wheel left/right <duty> <ms> | "
                   "wheel both <left> <right> <ms> | "
                   "wheel speed left/right <counts> <ms> | "
                   "wheel speed both <left_counts> <right_counts> <ms>\r\n");
    }
    else if (strcmp(line, "ping") == 0)
    {
        write_text("OK pong\r\n");
    }
    else if (strcmp(line, "status") == 0)
    {
        report_status();
    }
    else if (strcmp(line, "get report_ms") == 0)
    {
        report_parameter();
    }
    else if (strncmp(line, "set report_ms ", 14U) == 0)
    {
        uint32_t value;
        if (!parse_report_ms(line + 14U, &value))
        {
            write_text("ERR report_ms must be 0 or 100..60000\r\n");
            return;
        }
        report_ms = value;
        last_report_tick = osKernelGetTickCount();
        report_parameter();
    }
    else
    {
        write_text("ERR unknown command; use help\r\n");
    }
}

void debug_console_run(void)
{
    char line[DEBUG_CONSOLE_LINE_CAPACITY];
    size_t length = 0U;
    bool discard_line = false;

    if (!debug_uart_start())
    {
        Error_Handler();
    }
    write_text("READY gc-rescue-robot debug uart5 115200 8N1; use help\r\n");

    for (;;)
    {
        uint8_t byte;
        bool data_lost;
        if (debug_uart_read(&byte, &data_lost, 10U))
        {
            if (data_lost)
            {
                discard_line = true;
            }
            if (byte == '\r' || byte == '\n')
            {
                if (discard_line)
                {
                    write_text("ERR line discarded; resend command\r\n");
                }
                else
                {
                    line[length] = '\0';
                    execute_line(line);
                }
                length = 0U;
                discard_line = false;
            }
            else if (!discard_line)
            {
                if (byte == '\b' || byte == 0x7FU)
                {
                    if (length > 0U)
                    {
                        length--;
                    }
                }
                else if (byte == '\t' || (byte >= 0x20U && byte <= 0x7EU))
                {
                    if (length + 1U < sizeof(line))
                    {
                        line[length++] = (char)byte;
                    }
                    else
                    {
                        discard_line = true;
                    }
                }
                else
                {
                    discard_line = true;
                }
            }
        }

        const uint32_t tick = osKernelGetTickCount();
        if (report_ms != 0U && tick - last_report_tick >= report_ms)
        {
            last_report_tick = tick;
            report_status();
        }
    }
}
