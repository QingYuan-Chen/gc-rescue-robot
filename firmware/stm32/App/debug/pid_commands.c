#include "pid_commands.h"

#include "config/wheel_config.h"
#include "control/wheel_drive.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t draft_kp_milli;
static uint32_t draft_ki_milli;
static bool draft_ready;
static bool awaiting_result;
static uint32_t pending_id;

void pid_commands_init(void)
{
    draft_kp_milli = 0U;
    draft_ki_milli = 0U;
    draft_ready = false;
    awaiting_result = false;
    pending_id = 0U;
}

/* 十进制定点输入，最多三位小数；先限制整数大小，不使用浮点解析。 */
static bool parse_gain(const char *text, uint32_t *milli)
{
    while (*text == ' ')
    {
        text++;
    }
    uint32_t whole = 0U;
    uint32_t fraction = 0U;
    if (*text < '0' || *text > '9')
    {
        return false;
    }
    while (*text >= '0' && *text <= '9')
    {
        whole = whole * 10U + (uint32_t)(*text++ - '0');
        if (whole > WHEEL_PI_MAX_GAIN_MILLI / 1000U)
        {
            return false;
        }
    }
    if (*text == '.')
    {
        text++;
        uint32_t place = 100U;
        if (*text < '0' || *text > '9')
        {
            return false;
        }
        while (*text >= '0' && *text <= '9')
        {
            if (place == 0U)
            {
                return false;
            }
            fraction += (uint32_t)(*text++ - '0') * place;
            place /= 10U;
        }
    }
    *milli = whole * 1000U + fraction;
    return *text == '\0' && *milli <= WHEEL_PI_MAX_GAIN_MILLI;
}

bool pid_commands_execute(const char *line, char *response, size_t capacity)
{
    if (strncmp(line, "pid ", 4U) != 0 && strcmp(line, "pid") != 0)
    {
        return false;
    }
    wheel_drive_status_t status;
    wheel_drive_get_status(&status);
    if (!status.ready)
    {
        snprintf(response, capacity, "ERR pid not_ready\r\n");
        return true;
    }
    const pid_pi_params_t active = status.pi[WHEEL_LEFT].params;
    if (strcmp(line, "pid get left") == 0)
    {
        snprintf(response, capacity,
                 "OK pid left active kp=%.3f ki=%.3f kd=0.000 d_enabled=0 "
                 "tick_ms=%lu pending=%u\r\n",
                 (double)active.kp, (double)active.ki, (unsigned long)status.sample_tick_ms,
                 (unsigned)awaiting_result);
        return true;
    }
    if (awaiting_result)
    {
        snprintf(response, capacity, "ERR pid busy; wait for applied or rejected\r\n");
        return true;
    }
    if (!draft_ready)
    {
        draft_kp_milli = (uint32_t)((double)active.kp * 1000.0 + 0.5);
        draft_ki_milli = (uint32_t)((double)active.ki * 1000.0 + 0.5);
        draft_ready = true;
    }
    if (strcmp(line, "pid apply left") == 0)
    {
        const wheel_request_result_t result =
            wheel_drive_request_left_pi(draft_kp_milli, draft_ki_milli, &pending_id);
        if (result == WHEEL_REQUEST_ACCEPTED)
        {
            awaiting_result = true;
            snprintf(response, capacity, "OK pid left queued id=%lu\r\n",
                     (unsigned long)pending_id);
        }
        else
        {
            static const char *const reasons[] = {
                "accepted", "range", "not_ready", "stop_pending", "period_fault_reset_required",
                "busy"};
            snprintf(response, capacity, "ERR pid %s\r\n", reasons[result]);
        }
        return true;
    }
    if (strncmp(line, "pid edit left ", 14U) == 0)
    {
        const char *argument = line + 14U;
        const bool kp = strncmp(argument, "kp", 2U) == 0;
        const bool ki = strncmp(argument, "ki", 2U) == 0;
        const bool kd = strncmp(argument, "kd", 2U) == 0;
        uint32_t value;
        /* 滑块用冒号分隔字段和值，手工输入也可使用空格；回执仍不含冒号。 */
        if ((kp || ki || kd) && (argument[2] == ':' || argument[2] == ' ') &&
            parse_gain(argument + 3U, &value))
        {
            if (kd && value != 0U)
            {
                snprintf(response, capacity, "ERR pid kd_unsupported controller=pi kd=0.000\r\n");
                return true;
            }
            if (kp)
            {
                draft_kp_milli = value;
            }
            else if (ki)
            {
                draft_ki_milli = value;
            }
            snprintf(response, capacity,
                     "OK pid left draft kp=%.3f ki=%.3f kd=0.000; use pid apply left\r\n",
                     (double)draft_kp_milli / 1000.0, (double)draft_ki_milli / 1000.0);
            return true;
        }
    }
    snprintf(response, capacity,
             "ERR pid edit left <kp|ki|kd> <0..10000.000> | apply left | get left; "
             "max_decimals=3 kd=0 only\r\n");
    return true;
}

bool pid_commands_poll(char *response, size_t capacity)
{
    if (!awaiting_result)
    {
        return false;
    }
    wheel_drive_status_t status;
    wheel_drive_get_status(&status);
    if (status.left_pi_request_id != pending_id)
    {
        return false;
    }
    awaiting_result = false;
    if (status.left_pi_update_result == WHEEL_PI_UPDATE_APPLIED)
    {
        snprintf(response, capacity,
                 "OK pid left applied id=%lu tick_ms=%lu kp=%.3f ki=%.3f kd=0.000 d_enabled=0\r\n",
                 (unsigned long)pending_id, (unsigned long)status.left_pi_update_tick_ms,
                 (double)status.pi[WHEEL_LEFT].params.kp, (double)status.pi[WHEEL_LEFT].params.ki);
    }
    else
    {
        snprintf(response, capacity, "ERR pid left rejected id=%lu period_fault_reset_required\r\n",
                 (unsigned long)pending_id);
    }
    return true;
}
