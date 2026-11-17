#include "motor_commands.h"

#include "runtime/wheel_io.h"
#include "config/wheel_config.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

void motor_commands_status(char *response, size_t capacity)
{
    /* 只读取一个完整周期快照，不在格式化时重新访问编码器或执行器。 */
    wheel_io_status_t status;
    wheel_io_get_status(&status);
    snprintf(response, capacity,
             "OK motor ready=%u fault=%u left=M2 right=M1 request_left=%d request_right=%d "
             "applied_left=%d applied_right=%d phase_left=%u phase_right=%u remaining_ms=%lu "
             "sample_ms=%lu interval_ms=%lu counter_left=%u counter_right=%u "
             "delta_left=%ld delta_right=%ld total_left=%" PRId64 " total_right=%" PRId64 "\r\n",
             status.ready ? 1U : 0U, status.fault ? 1U : 0U, status.requested_left,
             status.requested_right, status.motor.applied_left, status.motor.applied_right,
             (unsigned)status.motor.phase_left, (unsigned)status.motor.phase_right,
             (unsigned long)status.remaining_ms, (unsigned long)status.sample_ms,
             (unsigned long)status.interval_ms, status.encoder.counter_left,
             status.encoder.counter_right, (long)status.encoder.delta_left,
             (long)status.encoder.delta_right, status.encoder.total_left,
             status.encoder.total_right);
}

/* 仅接受完整的十进制整数 token；拒绝单位后缀、十六进制和超长数值。
 * 中间值每步限到 100000，下一步乘十仍在 int 范围内；物理限幅另行检查。
 */
static bool parse_integer(const char **text, int *value)
{
    const char *p = *text;
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }
    bool negative = *p == '-';
    if (negative)
    {
        p++;
    }
    if (*p < '0' || *p > '9')
    {
        return false;
    }
    int parsed = 0;
    while (*p >= '0' && *p <= '9')
    {
        parsed = parsed * 10 + (*p++ - '0');
        if (parsed > 100000)
        {
            return false;
        }
    }
    if (*p != '\0' && *p != ' ' && *p != '\t')
    {
        return false;
    }
    *value = negative ? -parsed : parsed;
    *text = p;
    return true;
}

bool motor_commands_execute(const char *line, char *response, size_t capacity)
{
    if (strcmp(line, "encoder") == 0 || strcmp(line, "motor status") == 0)
    {
        motor_commands_status(response, capacity);
        return true;
    }
    if (strcmp(line, "motor stop") == 0)
    {
        wheel_io_stop();
        snprintf(response, capacity, "OK motor stop queued\r\n");
        return true;
    }
    if (strncmp(line, "motor ", 6U) != 0)
    {
        return false;
    }
    const char *values;
    /* 默认另一轮为零，实现单轮命令的显式隔离，不继承上一条双轮请求。 */
    int left = 0;
    int right = 0;
    int duration;
    bool valid = false;
    if (strncmp(line + 6, "left ", 5U) == 0)
    {
        values = line + 11;
        valid = parse_integer(&values, &left) && parse_integer(&values, &duration);
    }
    else if (strncmp(line + 6, "right ", 6U) == 0)
    {
        values = line + 12;
        valid = parse_integer(&values, &right) && parse_integer(&values, &duration);
    }
    else if (strncmp(line + 6, "both ", 5U) == 0)
    {
        values = line + 11;
        valid = parse_integer(&values, &left) && parse_integer(&values, &right) &&
                parse_integer(&values, &duration);
    }
    else
    {
        values = "invalid";
    }
    while (*values == ' ' || *values == '\t')
    {
        values++;
    }
    /* 先验证整行再转换到窄类型，避免溢出后的值被误当成合法占空比。 */
    if (!valid || *values != '\0' || left < -WHEEL_COMMAND_MAX_DUTY ||
        left > WHEEL_COMMAND_MAX_DUTY || right < -WHEEL_COMMAND_MAX_DUTY ||
        right > WHEEL_COMMAND_MAX_DUTY || duration < (int)WHEEL_SAMPLE_PERIOD_MS ||
        duration > (int)WHEEL_COMMAND_MAX_DURATION_MS)
    {
        snprintf(response, capacity,
                 "ERR motor use left/right <duty> <ms> or both <left> <right> <ms>; "
                 "duty=-%d..%d ms=%u..%u\r\n",
                 WHEEL_COMMAND_MAX_DUTY, WHEEL_COMMAND_MAX_DUTY, WHEEL_SAMPLE_PERIOD_MS,
                 WHEEL_COMMAND_MAX_DURATION_MS);
    }
    else if (!wheel_io_request((int16_t)left, (int16_t)right, (uint32_t)duration))
    {
        snprintf(response, capacity, "ERR motor not ready, fault or stop pending\r\n");
    }
    else
    {
        snprintf(response, capacity, "OK motor queued left=%d right=%d duration_ms=%d\r\n", left,
                 right, duration);
    }
    return true;
}
