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
             "duty_a=%ld duty_b=%ld delta_a=%ld delta_b=%ld "
             "total_a=%lld total_b=%lld raw_a=%lu raw_b=%lu\r\n",
             (unsigned long)status.sample_tick_ms, WHEEL_PERIOD_MS,
             (unsigned long)status.remaining_ms, (long)status.duty_permille[WHEEL_A],
             (long)status.duty_permille[WHEEL_B], (long)status.delta_counts[WHEEL_A],
             (long)status.delta_counts[WHEEL_B], (long long)status.total_counts[WHEEL_A],
             (long long)status.total_counts[WHEEL_B], (unsigned long)status.raw_counts[WHEEL_A],
             (unsigned long)status.raw_counts[WHEEL_B]);
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
    const bool wheel_a = strncmp(line, "wheel a ", 8U) == 0;
    const bool wheel_b = strncmp(line, "wheel b ", 8U) == 0;
    if (both || wheel_a || wheel_b)
    {
        const char *cursor = line + (both ? 11U : 8U);
        int32_t first, second = 0, duration;
        if (parse_integer(&cursor, &first) && (!both || parse_integer(&cursor, &second)) &&
            parse_integer(&cursor, &duration) && *cursor == '\0' && duration > 0 &&
            wheel_drive_request(wheel_b ? 0 : first, wheel_b ? first : second, (uint32_t)duration))
        {
            snprintf(response, capacity, "OK wheel command queued\r\n");
            return true;
        }
    }
    snprintf(response, capacity,
             "ERR wheel: status | stop | a/b <duty> <ms> | both <a> <b> <ms>; "
             "duty=-%d..%d ms=%u..%u step=%u\r\n",
             WHEEL_MANUAL_MAX_DUTY_PERMILLE, WHEEL_MANUAL_MAX_DUTY_PERMILLE, WHEEL_PERIOD_MS,
             WHEEL_MANUAL_MAX_DURATION_MS, WHEEL_PERIOD_MS);
    return true;
}
