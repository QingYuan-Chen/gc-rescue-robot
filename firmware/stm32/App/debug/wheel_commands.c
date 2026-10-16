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
    static const char *const reasons[] = {
        "accepted", "range", "not_ready", "stop_pending", "period_fault_reset_required", "busy"};
    if (result == WHEEL_REQUEST_ACCEPTED)
    {
        snprintf(response, capacity, "OK wheel command queued\r\n");
    }
    else
    {
        snprintf(response, capacity, "ERR wheel %s\r\n", reasons[result]);
    }
}

static bool parse_selection(const char **text, wheel_drive_selection_t *selection)
{
    static const char *const names[] = {"both", "left", "right"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        const size_t length = strlen(names[i]);
        if (strncmp(*text, names[i], length) == 0)
        {
            const char delimiter = (*text)[length];
            if (delimiter == '\0' || delimiter == ' ' || delimiter == ':')
            {
                *selection = (wheel_drive_selection_t)i;
                *text += length;
                return true;
            }
        }
    }
    return false;
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

    const bool target = strncmp(line, "wheel target ", 13U) == 0;
    const bool start = strncmp(line, "wheel start ", 12U) == 0;
    const char *cursor = line + (target ? 13U : (start ? 12U : 5U));
    if (!target && !start && *cursor == ' ')
    {
        cursor++;
    }
    wheel_drive_selection_t selection;
    if (parse_selection(&cursor, &selection))
    {
        if (start && *cursor == '\0')
        {
            format_result(wheel_drive_start_speed(selection), response, capacity);
            return true;
        }
        /* 数值控件可用冒号传目标；开环仍保留原来的占空比和时长两个输入。 */
        if (!start && (*cursor == ' ' || (target && *cursor == ':')))
        {
            cursor++;
            int32_t first, second = 0, duration = 0;
            const bool both = selection == WHEEL_SELECT_BOTH;
            const bool right = selection == WHEEL_SELECT_RIGHT;
            if (parse_number(&cursor, target, &first) &&
                (!both || parse_number(&cursor, target, &second)) &&
                (target || (parse_number(&cursor, false, &duration) && duration > 0)) &&
                *cursor == '\0')
            {
                const int32_t left_value = right ? 0 : first;
                const int32_t right_value = right ? first : second;
                const wheel_request_result_t result =
                    target ? wheel_drive_set_speed_target(selection, left_value, right_value)
                           : wheel_drive_request_open_loop(left_value, right_value,
                                                           (uint32_t)duration);
                format_result(result, response, capacity);
                return true;
            }
        }
    }
    snprintf(response, capacity,
             "ERR wheel status | stop | target left/right <counts> | "
             "target both <left> <right> | start left/right/both | "
             "left/right <duty> <ms> | both <left_duty> <right_duty> <ms>; "
             "duty=-%d..%d; target=counts/10ms, max_milli_counts=%d, max_decimals=3; "
             "open_ms=%u..%u step=%u; parameter separator space or colon\r\n",
             WHEEL_MANUAL_MAX_DUTY_PERMILLE, WHEEL_MANUAL_MAX_DUTY_PERMILLE,
             WHEEL_SPEED_MAX_TARGET_MILLI_COUNTS, WHEEL_PERIOD_MS, WHEEL_OPEN_LOOP_MAX_DURATION_MS,
             WHEEL_PERIOD_MS);
    return true;
}
