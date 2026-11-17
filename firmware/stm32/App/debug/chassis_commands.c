#include "chassis_commands.h"

#include "config/chassis_config.h"
#include "runtime/chassis.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool parse_number(const char *text, float *value)
{
    char *end = NULL;
    errno = 0;
    const float number = strtof(text, &end);
    if (errno != 0 || end == text || end == NULL || *end != '\0' || !isfinite(number))
    {
        return false;
    }
    *value = number;
    return true;
}

static bool parse_duration(const char *text, uint32_t *value)
{
    char *end = NULL;
    errno = 0;
    const unsigned long number = strtoul(text, &end, 10);
    if (errno != 0 || end == text || end == NULL || *end != '\0' ||
        number < CHASSIS_COMMAND_MIN_MS || number > CHASSIS_COMMAND_MAX_MS)
    {
        return false;
    }
    *value = (uint32_t)number;
    return true;
}

static void report(char *response, size_t capacity)
{
    chassis_status_t status;
    chassis_get_status(&status);
    snprintf(response, capacity,
             "OK chassis mode=%u v_mm_s=%ld omega_mdeg_s=%ld i_mdps=%ld "
             "left_mrpm=%ld right_mrpm=%ld "
             "target_yaw_mdeg=%ld yaw_mdeg=%ld error_mdeg=%ld rate_mdps=%ld "
             "distance_mm=%ld distance_target_mm=%ld remaining_ms=%lu "
             "saturated=%u imu_valid=%u settled=%u stop_reason=%u\r\n",
             (unsigned)status.mode, (long)(status.v_mm_s * 1000.0f),
             (long)(status.omega_deg_s * 1000.0f), (long)(status.i_dps * 1000.0f),
             (long)(status.left_rpm * 1000.0f),
             (long)(status.right_rpm * 1000.0f), (long)(status.target_yaw_deg * 1000.0f),
             (long)(status.yaw_deg * 1000.0f), (long)(status.yaw_error_deg * 1000.0f),
             (long)(status.yaw_rate_dps * 1000.0f), (long)(status.distance_mm * 1000.0f),
             (long)(status.distance_target_mm * 1000.0f), (unsigned long)status.remaining_ms,
             status.saturated ? 1U : 0U, status.imu_valid ? 1U : 0U,
             status.settled ? 1U : 0U, status.stop_reason);
}

static bool set_heading_parameter(const char *name, const char *text,
                                  chassis_hold_config_t *config, bool allow_integral)
{
    float value;
    if (strcmp(name, "settle") == 0)
    {
        float count;
        if (!parse_number(text, &count) || truncf(count) != count || count < 10.0f ||
            count > 1000.0f) { return false; }
        config->settle_ms = (uint32_t)count;
        return true;
    }
    if (!parse_number(text, &value)) { return false; }
    if (strcmp(name, "kp") == 0) { config->kp = value; }
    else if (strcmp(name, "kd") == 0) { config->kd = value; }
    else if (strcmp(name, "filter") == 0) { config->filter_ms = value; }
    else if (strcmp(name, "wmax") == 0) { config->wmax_dps = value; }
    else if (strcmp(name, "accel") == 0) { config->accel_dps2 = value; }
    else if (strcmp(name, "tol") == 0) { config->tol_deg = value; }
    else if (strcmp(name, "rate_tol") == 0) { config->rate_tol_dps = value; }
    else if (allow_integral && strcmp(name, "ki") == 0) { config->ki = value; }
    else if (allow_integral && strcmp(name, "izone") == 0) { config->izone_deg = value; }
    else if (allow_integral && strcmp(name, "ilimit") == 0) { config->ilimit_dps = value; }
    else { return false; }
    return true;
}

static void report_hold(char *response, size_t capacity)
{
    chassis_hold_config_t config;
    chassis_get_hold_config(&config);
    snprintf(response, capacity,
             "OK hold kp_m=%ld kd_m=%ld ki_m=%ld filter_mms=%ld wmax_mdps=%ld "
             "accel_mdps2=%ld tol_mdeg=%ld rate_tol_mdps=%ld izone_mdeg=%ld "
             "ilimit_mdps=%ld settle_ms=%lu\r\n",
             (long)(config.kp * 1000.0f), (long)(config.kd * 1000.0f),
             (long)(config.ki * 1000.0f), (long)(config.filter_ms * 1000.0f),
             (long)(config.wmax_dps * 1000.0f), (long)(config.accel_dps2 * 1000.0f),
             (long)(config.tol_deg * 1000.0f), (long)(config.rate_tol_dps * 1000.0f),
             (long)(config.izone_deg * 1000.0f), (long)(config.ilimit_dps * 1000.0f),
             (unsigned long)config.settle_ms);
}

bool chassis_commands_execute(char *line, char *response, size_t capacity)
{
    const bool prefixed = strncmp(line, "chassis ", 8U) == 0;
    const bool turn_alias = strncmp(line, "turn ", 5U) == 0;
    const bool straight_alias = strncmp(line, "straight ", 9U) == 0;
    const bool hold_alias = strncmp(line, "hold ", 5U) == 0;
    if (!prefixed && !turn_alias && !straight_alias && !hold_alias) { return false; }

    /* VOFA 未绑定命令的滑块发送“名称:数值”，转换成空格分隔的参数。 */
    char *colon = strchr(line, ':');
    if (colon != NULL)
    {
        static const char *const sliders[] = {
            "chassis turn", "turn set kp", "turn set kd", "turn set filter",
            "turn set wmax", "turn set accel", "turn set tol", "turn set rate_tol",
            "turn set settle", "hold set kp", "hold set kd", "hold set filter",
            "hold set wmax", "hold set accel", "hold set tol", "hold set rate_tol",
            "hold set settle", "hold set ki", "hold set izone", "hold set ilimit",
            "chassis turn set kp", "chassis turn set kd",
            "chassis turn set filter", "chassis turn set wmax", "chassis turn set accel",
            "chassis turn set tol", "chassis turn set rate_tol", "chassis turn set settle",
            "chassis hold set kp", "chassis hold set kd", "chassis hold set filter",
            "chassis hold set wmax", "chassis hold set accel", "chassis hold set tol",
            "chassis hold set rate_tol", "chassis hold set settle",
            "chassis hold set ki", "chassis hold set izone", "chassis hold set ilimit"};
        const size_t length = (size_t)(colon - line);
        bool matched = false;
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
            snprintf(response, capacity, "ERR invalid chassis slider command\r\n");
            return true;
        }
        *colon = ' ';
    }

    char *command = prefixed ? line + 8 : line;
    char *save = NULL;
    char *action = strtok_r(command, " \t", &save);
    char *first = strtok_r(NULL, " \t", &save);
    char *second = strtok_r(NULL, " \t", &save);
    char *third = strtok_r(NULL, " \t", &save);
    char *fourth = strtok_r(NULL, " \t", &save);
    if (action == NULL) { snprintf(response, capacity, "ERR chassis command\r\n"); return true; }

    if (strcmp(action, "vw") == 0 && first != NULL && second != NULL && third != NULL &&
        fourth == NULL)
    {
        float v_mm_s, omega_deg_s;
        uint32_t duration_ms;
        if (!parse_number(first, &v_mm_s) || !parse_number(second, &omega_deg_s) ||
            !parse_duration(third, &duration_ms) ||
            !chassis_start_vw(v_mm_s, omega_deg_s, duration_ms))
        {
            snprintf(response, capacity, "ERR chassis vw arguments or state\r\n");
            return true;
        }
        report(response, capacity);
        return true;
    }
    if (strcmp(action, "turn") == 0 && first != NULL && second == NULL && third == NULL)
    {
        float degrees;
        if (!parse_number(first, &degrees) || !chassis_start_turn(degrees))
        {
            snprintf(response, capacity, "ERR chassis turn arguments or state\r\n");
            return true;
        }
        report(response, capacity);
        return true;
    }
    if (strcmp(action, "straight") == 0 && first != NULL && second != NULL && fourth == NULL)
    {
        float v_mm_s, distance_mm = 0.0f;
        uint32_t duration_ms;
        if (!parse_number(first, &v_mm_s) || !parse_duration(second, &duration_ms) ||
            (third != NULL && !parse_number(third, &distance_mm)) ||
            !chassis_start_straight(v_mm_s, duration_ms, distance_mm))
        {
            snprintf(response, capacity, "ERR chassis straight arguments or state\r\n");
            return true;
        }
        report(response, capacity);
        return true;
    }
    if (strcmp(action, "turn") == 0 && first != NULL && strcmp(first, "set") == 0 &&
        second != NULL && third != NULL && fourth == NULL)
    {
        chassis_turn_config_t config;
        chassis_get_turn_config(&config);
        if (!set_heading_parameter(second, third, &config, false) ||
            !chassis_set_turn_config(&config))
        {
            snprintf(response, capacity, "ERR turn parameter or range\r\n");
            return true;
        }
        snprintf(response, capacity, "OK turn set %s=%s\r\n", second, third);
        return true;
    }
    if (strcmp(action, "hold") == 0 && first != NULL && strcmp(first, "set") == 0 &&
        second != NULL && third != NULL && fourth == NULL)
    {
        chassis_hold_config_t config;
        chassis_get_hold_config(&config);
        if (!set_heading_parameter(second, third, &config, true) ||
            !chassis_set_hold_config(&config))
        {
            snprintf(response, capacity, "ERR hold parameter or range\r\n");
            return true;
        }
        snprintf(response, capacity, "OK hold set %s=%s\r\n", second, third);
        return true;
    }
    if (strcmp(action, "hold") == 0 && first != NULL && strcmp(first, "get") == 0 &&
        second == NULL)
    {
        report_hold(response, capacity);
        return true;
    }
    if (strcmp(action, "stop") == 0 && first == NULL)
    {
        chassis_stop();
        snprintf(response, capacity, "OK chassis stop\r\n");
        return true;
    }
    if (strcmp(action, "get") == 0 && first == NULL)
    {
        report(response, capacity);
        return true;
    }
    snprintf(response, capacity, "ERR unknown chassis command\r\n");
    return true;
}
