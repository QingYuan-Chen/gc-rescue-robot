#include "wheel_commands.h"
#include "wheel_status.h"

#include "config/wheel_config.h"
#include "control/wheel_drive.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 调试数值只接受十进制定点文本，不引入 NaN、指数或依赖 locale 的浮点解析。 */
static bool parse_number(const char **text, bool fractional, int32_t *value)
{
    const char *cursor = *text;
    while (*cursor == ' ')
    {
        cursor++;
    }
    const bool negative = *cursor == '-';
    if (negative)
    {
        cursor++;
    }
    if (*cursor < '0' || *cursor > '9')
    {
        return false;
    }
    int32_t whole = 0;
    while (*cursor >= '0' && *cursor <= '9')
    {
        whole = whole * 10 + (*cursor++ - '0');
        if (whole > 10000)
        {
            return false;
        }
    }
    int32_t fraction = 0;
    if (fractional && *cursor == '.')
    {
        cursor++;
        int32_t place = 100;
        if (*cursor < '0' || *cursor > '9')
        {
            return false;
        }
        while (*cursor >= '0' && *cursor <= '9')
        {
            if (place == 0)
            {
                return false;
            }
            fraction += (*cursor++ - '0') * place;
            place /= 10;
        }
    }
    if (*cursor != ' ' && *cursor != '\0')
    {
        return false;
    }
    const int32_t magnitude = fractional ? whole * 1000 + fraction : whole;
    *value = negative ? -magnitude : magnitude;
    *text = cursor;
    return true;
}

static void format_result(wheel_request_result_t result, char *response, size_t capacity)
{
    static const char *const reasons[] = {"accepted", "range", "not_ready", "stop_pending",
                                          "period_fault_reset_required"};
    if (result == WHEEL_REQUEST_ACCEPTED)
    {
        snprintf(response, capacity, "OK wheel command queued\r\n");
    }
    else
    {
        snprintf(response, capacity, "ERR wheel %s\r\n", reasons[result]);
    }
}

bool wheel_commands_execute(const char *line, char *response, size_t capacity)
{
    if (strncmp(line, "wheel ", 6U) != 0 && strcmp(line, "wheel") != 0)
    {
        return false;
    }
    if (strcmp(line, "wheel status") == 0)
    {
        wheel_status_format(response, capacity);
        return true;
    }
    if (strcmp(line, "wheel stop") == 0)
    {
        wheel_drive_stop();
        snprintf(response, capacity, "OK wheel stop queued\r\n");
        return true;
    }

    const bool speed = strncmp(line, "wheel speed ", 12U) == 0;
    const char *arguments = line + (speed ? 12U : 5U);
    if (!speed && *arguments == ' ')
    {
        arguments++;
    }
    const bool both = strncmp(arguments, "both ", 5U) == 0;
    const bool left = strncmp(arguments, "left ", 5U) == 0;
    const bool right = strncmp(arguments, "right ", 6U) == 0;
    if (both || left || right)
    {
        const char *cursor = arguments + (right ? 6U : 5U);
        int32_t first, second = 0, duration;
        if (parse_number(&cursor, speed, &first) &&
            (!both || parse_number(&cursor, speed, &second)) &&
            parse_number(&cursor, false, &duration) && *cursor == '\0' && duration > 0)
        {
            const int32_t left_value = right ? 0 : first;
            const int32_t right_value = right ? first : second;
            const wheel_drive_selection_t selection =
                both ? WHEEL_SELECT_BOTH : (left ? WHEEL_SELECT_LEFT : WHEEL_SELECT_RIGHT);
            const wheel_request_result_t result =
                speed ? wheel_drive_request_speed(selection, left_value, right_value,
                                                  (uint32_t)duration)
                      : wheel_drive_request_open_loop(left_value, right_value, (uint32_t)duration);
            format_result(result, response, capacity);
            return true;
        }
    }
    snprintf(response, capacity,
             "ERR wheel: status | stop | [speed] left/right <value> <ms> | "
             "[speed] both <left> <right> <ms>; duty=-%d..%d; speed=counts/10ms, "
             "max_milli_counts=%d, max_decimals=3; ms=%u..%u step=%u\r\n",
             WHEEL_MANUAL_MAX_DUTY_PERMILLE, WHEEL_MANUAL_MAX_DUTY_PERMILLE,
             WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS, WHEEL_PERIOD_MS, WHEEL_MANUAL_MAX_DURATION_MS,
             WHEEL_PERIOD_MS);
    return true;
}
