#include "position_commands.h"
#include "runtime/wheel_io.h"
#include "config/wheel_config.h"

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_number(const char *text, float *value)
{
    char *end;
    errno = 0;
    const float number = strtof(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(number))
    {
        return false;
    }
    *value = number;
    return true;
}

static bool parse_counts(const char *text, int32_t *value)
{
    float number;
    if (!parse_number(text, &number) || number < -WHEEL_POSITION_MAX_COUNTS ||
        number > WHEEL_POSITION_MAX_COUNTS || truncf(number) != number) { return false; }
    *value = (int32_t)number;
    return true;
}

bool position_commands_execute(char *line, char *response, size_t capacity)
{
    if (strncmp(line, "pos ", 4U) != 0) { return false; }
    char *colon = strchr(line, ':');
    if (colon != NULL)
    {
        static const char *const sliders[] = {"pos target left", "pos target let",
            "pos set kp", "pos set ki", "pos set kd", "pos set limit", "pos set tolerance"};
        bool matched = false;
        const size_t length = (size_t)(colon - line);
        for (unsigned i = 0; i < sizeof(sliders) / sizeof(sliders[0]); i++)
        {
            if (strlen(sliders[i]) == length && strncmp(line, sliders[i], length) == 0)
            {
                matched = true;
                break;
            }
        }
        if (!matched || colon[1] == '\0' || strchr(colon + 1, ':') != NULL)
        {
            snprintf(response, capacity, "ERR invalid slider command\r\n");
            return true;
        }
        *colon = ' ';
    }
    if (strcmp(line, "pos get") == 0)
    {
        static const char *const states[] = {"idle", "queued", "running", "done", "stopped",
                                             "timeout", "overshoot", "fault"};
        wheel_position_status_t status;
        wheel_position_config_t config;
        wheel_io_position_get(&status);
        wheel_io_position_get_config(&config);
        snprintf(response, capacity,
                 "OK pos id=%lu state=%s elapsed_ms=%lu target=%ld,%ld actual=%" PRId64 ",%" PRId64
                 " error=%" PRId64 ",%" PRId64 " target_mrpm=%ld,%ld corrections=%u,%u "
                 "kp_millionths=%ld ki_millionths=%ld kd_millionths=%ld limit_mrpm=%ld tolerance=%ld\r\n",
                 (unsigned long)status.id, states[status.state], (unsigned long)status.elapsed_ms,
                 (long)status.target[0], (long)status.target[1], status.actual[0], status.actual[1],
                 status.error[0], status.error[1], (long)(status.target_rpm[0] * 1000.0f),
                 (long)(status.target_rpm[1] * 1000.0f), status.corrections[0],
                 status.corrections[1], (long)(config.kp * 1000000.0f + 0.5f),
                 (long)(config.ki * 1000000.0f + 0.5f), (long)(config.kd * 1000000.0f + 0.5f),
                 (long)(config.max_rpm * 1000.0f + 0.5f), (long)config.tolerance_counts);
        return true;
    }
    char *save = NULL;
    char *token = strtok_r(line, " \t", &save);
    char *action = strtok_r(NULL, " \t", &save);
    char *left = strtok_r(NULL, " \t", &save);
    char *right = strtok_r(NULL, " \t", &save);
    char *extra = strtok_r(NULL, " \t", &save);
    int32_t left_counts, right_counts;
    uint32_t id;
    if (action && strcmp(action, "target") == 0 && left && right && !extra &&
        (strcmp(left, "left") == 0 || strcmp(left, "let") == 0) &&
        parse_counts(right, &left_counts))
    {
        if (wheel_io_position_slider_left(left_counts, &id))
        {
            snprintf(response, capacity, "OK pos target_left=%ld id=%lu\r\n",
                     (long)left_counts, (unsigned long)id);
        }
        else { snprintf(response, capacity, "ERR pos target or state\r\n"); }
        return true;
    }
    if (action && strcmp(action, "set") == 0 && left && right && !extra)
    {
        wheel_position_config_t config;
        float value;
        wheel_io_position_get_config(&config);
        bool valid = parse_number(right, &value);
        if (valid && strcmp(left, "kp") == 0) { config.kp = value; }
        else if (valid && strcmp(left, "ki") == 0) { config.ki = value; }
        else if (valid && strcmp(left, "kd") == 0) { config.kd = value; }
        else if (valid && strcmp(left, "limit") == 0) { config.max_rpm = value; }
        else if (valid && strcmp(left, "tolerance") == 0 && value >= 1.0f &&
                 value <= 100.0f && truncf(value) == value) { config.tolerance_counts = (int32_t)value; }
        else { valid = false; }
        if (valid && wheel_io_position_set_config(&config))
        {
            snprintf(response, capacity, "OK pos config kp_millionths=%ld ki_millionths=%ld kd_millionths=%ld limit_mrpm=%ld tolerance=%ld\r\n",
                     (long)(config.kp * 1000000.0f + 0.5f),
                     (long)(config.ki * 1000000.0f + 0.5f), (long)(config.kd * 1000000.0f + 0.5f),
                     (long)(config.max_rpm * 1000.0f + 0.5f),
                     (long)config.tolerance_counts);
        }
        else { snprintf(response, capacity, "ERR pos parameter or state\r\n"); }
        return true;
    }
    if (token && action && left && right && !extra && strcmp(action, "move") == 0 &&
        parse_counts(left, &left_counts) && parse_counts(right, &right_counts) &&
        wheel_io_position_move(left_counts, right_counts, &id))
    {
        snprintf(response, capacity, "OK pos queued id=%lu\r\n", (unsigned long)id);
    }
    else { snprintf(response, capacity, "ERR pos move arguments or state\r\n"); }
    return true;
}
