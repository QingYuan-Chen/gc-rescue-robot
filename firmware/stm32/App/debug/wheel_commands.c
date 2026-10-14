#include "wheel_commands.h"

#include "config/wheel_config.h"
#include "control/wheel_drive.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool parse_integer(const char **text, int32_t *value)
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
    int32_t result = 0;
    while (*cursor >= '0' && *cursor <= '9')
    {
        result = result * 10 + (*cursor++ - '0');
        if (result > 10000)
        {
            return false;
        }
    }
    if (*cursor != ' ' && *cursor != '\0')
    {
        return false;
    }
    *value = negative ? -result : result;
    *text = cursor;
    return true;
}

void wheel_commands_format_status(char *response, size_t capacity)
{
    wheel_drive_status_t status;
    wheel_drive_get_status(&status);
    snprintf(response, capacity,
             "OK wheel tick_ms=%lu period_ms=%u remaining_ms=%lu "
             "duty_left=%ld duty_right=%ld delta_left=%ld delta_right=%ld "
             "total_left=%lld total_right=%lld raw_left=%lu raw_right=%lu\r\n",
             (unsigned long)status.sample_tick_ms, WHEEL_PERIOD_MS,
             (unsigned long)status.remaining_ms, (long)status.duty_permille[WHEEL_LEFT],
             (long)status.duty_permille[WHEEL_RIGHT], (long)status.delta_counts[WHEEL_LEFT],
             (long)status.delta_counts[WHEEL_RIGHT], (long long)status.total_counts[WHEEL_LEFT],
             (long long)status.total_counts[WHEEL_RIGHT],
             (unsigned long)status.raw_counts[WHEEL_LEFT],
             (unsigned long)status.raw_counts[WHEEL_RIGHT]);
}

bool wheel_commands_execute(const char *line, char *response, size_t capacity)
{
    if (strncmp(line, "wheel ", 6U) != 0 && strcmp(line, "wheel") != 0)
    {
        return false;
    }
    if (strcmp(line, "wheel status") == 0)
    {
        wheel_commands_format_status(response, capacity);
        return true;
    }
    if (strcmp(line, "wheel stop") == 0)
    {
        wheel_drive_stop();
        snprintf(response, capacity, "OK wheel stop queued\r\n");
        return true;
    }

    const bool both = strncmp(line, "wheel both ", 11U) == 0;
    const bool left = strncmp(line, "wheel left ", 11U) == 0;
    const bool right = strncmp(line, "wheel right ", 12U) == 0;
    if (both || left || right)
    {
        const char *cursor = line + (right ? 12U : 11U);
        int32_t first, second = 0, duration;
        if (parse_integer(&cursor, &first) && (!both || parse_integer(&cursor, &second)) &&
            parse_integer(&cursor, &duration) && *cursor == '\0' && duration > 0 &&
            wheel_drive_request(right ? 0 : first, right ? first : second, (uint32_t)duration))
        {
            snprintf(response, capacity, "OK wheel command queued\r\n");
            return true;
        }
    }
    snprintf(response, capacity,
             "ERR wheel: status | stop | left/right <duty> <ms> | both <left> <right> <ms>; "
             "duty=-%d..%d ms=%u..%u step=%u\r\n",
             WHEEL_MANUAL_MAX_DUTY_PERMILLE, WHEEL_MANUAL_MAX_DUTY_PERMILLE, WHEEL_PERIOD_MS,
             WHEEL_MANUAL_MAX_DURATION_MS, WHEEL_PERIOD_MS);
    return true;
}
