#include "speed_commands.h"

#include "runtime/wheel_io.h"
#include "bsp/debug_uart.h"
#include "cmsis_os2.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t view;
static uint32_t period_ms = 20U;
static uint32_t last_wave_ms;
static uint32_t dropped_waves;

static bool decimal(const char *s, float *value)
{
    if (s == NULL || *s == '\0') { return false; }
    bool negative = false;
    if (*s == '-' || *s == '+') { negative = *s++ == '-'; }
    bool digit = false;
    float whole = 0.0f;
    while (*s >= '0' && *s <= '9')
    {
        digit = true;
        whole = whole * 10.0f + (float)(*s++ - '0');
        if (whole > 1000000.0f) { return false; }
    }
    if (*s == '.')
    {
        s++;
        float place = 0.1f;
        unsigned count = 0U;
        while (*s >= '0' && *s <= '9')
        {
            if (++count > 6U) { return false; }
            digit = true;
            whole += (float)(*s++ - '0') * place;
            place *= 0.1f;
        }
    }
    if (!digit || *s != '\0') { return false; }
    *value = negative ? -whole : whole;
    return true;
}

static uint8_t wheels(const char *text)
{
    if (text == NULL) { return 0U; }
    if (strcmp(text, "left") == 0) { return 1U; }
    if (strcmp(text, "right") == 0) { return 2U; }
    if (strcmp(text, "both") == 0) { return 3U; }
    return 0U;
}

static bool parameter(const char *text, speed_param_t *result)
{
    if (text == NULL) { return false; }
    if (strcmp(text, "kp") == 0) { *result = SPEED_PARAM_KP; }
    else if (strcmp(text, "ki") == 0) { *result = SPEED_PARAM_KI; }
    else if (strcmp(text, "limit") == 0) { *result = SPEED_PARAM_LIMIT; }
    else if (strcmp(text, "ramp") == 0) { *result = SPEED_PARAM_RAMP; }
    else { return false; }
    return true;
}

static void fixed(char *text, size_t capacity, float value)
{
    /* 保留滑块发送的六位小数，避免依赖嵌入式 printf 浮点配置。 */
    const bool negative = value < 0.0f;
    const uint64_t micro = (uint64_t)(fabs((double)value) * 1000000.0 + 0.5);
    snprintf(text, capacity, "%s%" PRIu64 ".%06" PRIu64, negative ? "-" : "",
             micro / 1000000U, micro % 1000000U);
}

static void status_text(char *response, size_t capacity)
{
    wheel_io_status_t s;
    debug_uart_status_t uart;
    wheel_speed_config_t pending[2];
    wheel_io_get_status(&s);
    debug_uart_get_status(&uart);
    wheel_io_speed_get_staged(pending);
    char target[2][24], ref[2][24], feedback[2][24], error[2][24], p[2][24], integral[2][24];
    char free_output[2][24], kp[2][24], ki[2][24], ramp[2][24];
    char staged_kp[2][24], staged_ki[2][24], staged_ramp[2][24];
    for (unsigned i = 0; i < 2U; i++)
    {
        fixed(target[i], sizeof(target[i]), s.target_rpm[i]);
        fixed(ref[i], sizeof(ref[i]), s.speed[i].effective_rpm);
        fixed(feedback[i], sizeof(feedback[i]), s.speed[i].feedback_rpm);
        fixed(error[i], sizeof(error[i]), s.speed[i].error_rpm);
        fixed(p[i], sizeof(p[i]), s.speed[i].proportional);
        fixed(integral[i], sizeof(integral[i]), s.speed[i].integral);
        fixed(free_output[i], sizeof(free_output[i]), s.speed[i].unconstrained);
        fixed(kp[i], sizeof(kp[i]), s.speed[i].config.kp);
        fixed(ki[i], sizeof(ki[i]), s.speed[i].config.ki);
        fixed(ramp[i], sizeof(ramp[i]), s.speed[i].config.ramp_rpm_s);
        fixed(staged_kp[i], sizeof(staged_kp[i]), pending[i].kp);
        fixed(staged_ki[i], sizeof(staged_ki[i]), pending[i].ki);
        fixed(staged_ramp[i], sizeof(staged_ramp[i]), pending[i].ramp_rpm_s);
    }
    const int written = snprintf(response, capacity,
             "OK speed session=%u claimed=%u stop_pending=%u running=%u selected=%u fault=%u reason=%u dt_us=%lu "
             "applied_id=%lu applied_ms=%lu dropped=%lu\r\n"
             "OK left target=%s ref=%s rpm=%s error=%s p=%s i=%s free=%s req=%d out=%d "
             "phase=%u saturated=%u delta=%ld total=%" PRId64 " kp=%s ki=%s limit=%d ramp=%s configured=%u "
             "staged_kp=%s staged_ki=%s staged_limit=%d staged_ramp=%s\r\n"
             "OK right target=%s ref=%s rpm=%s error=%s p=%s i=%s free=%s req=%d out=%d "
             "phase=%u saturated=%u delta=%ld total=%" PRId64 " kp=%s ki=%s limit=%d ramp=%s configured=%u "
             "staged_kp=%s staged_ki=%s staged_limit=%d staged_ramp=%s\r\n",
             s.session, s.session_claimed, s.stop_pending, s.speed_running, s.selected, s.fault, s.stop_reason,
             (unsigned long)s.dt_us, (unsigned long)s.applied_id, (unsigned long)s.applied_ms,
             (unsigned long)(dropped_waves + uart.sample_cancelled_count),
             target[0], ref[0], feedback[0], error[0], p[0], integral[0], free_output[0],
             s.speed[0].requested, s.motor.applied_left, s.speed[0].phase, s.speed[0].saturated,
             (long)s.encoder.delta_left, s.encoder.total_left, kp[0], ki[0], s.speed[0].config.limit, ramp[0],
             s.speed[0].config.configured, staged_kp[0], staged_ki[0], pending[0].limit, staged_ramp[0],
             target[1], ref[1], feedback[1], error[1], p[1], integral[1], free_output[1],
             s.speed[1].requested, s.motor.applied_right, s.speed[1].phase, s.speed[1].saturated,
             (long)s.encoder.delta_right, s.encoder.total_right, kp[1], ki[1], s.speed[1].config.limit, ramp[1],
             s.speed[1].config.configured, staged_kp[1], staged_ki[1], pending[1].limit, staged_ramp[1]);
    if (written < 0 || (size_t)written >= capacity)
    {
        snprintf(response, capacity, "ERR speed status too long\r\n");
    }
}

bool speed_commands_execute(char *line, char *response, size_t capacity)
{
    if (strncmp(line, "speed ", 6U) != 0 && strncmp(line, "vofa ", 5U) != 0) { return false; }
    /* VOFA+ 未绑定命令的滑块发送“名称:数值”，只转换已定义的滑块命令。 */
    char *colon = strchr(line, ':');
    if (colon != NULL)
    {
        static const char *const sliders[] = {
            "speed target left", "speed target right",
            "speed set left kp", "speed set left ki", "speed set left limit", "speed set left ramp",
            "speed set right kp", "speed set right ki", "speed set right limit", "speed set right ramp"
        };
        bool matched = false;
        const size_t prefix_length = (size_t)(colon - line);
        for (unsigned i = 0U; i < sizeof(sliders) / sizeof(sliders[0]); i++)
        {
            if (strlen(sliders[i]) == prefix_length && strncmp(line, sliders[i], prefix_length) == 0)
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
    char *save = NULL;
    char *tokens[6] = {0};
    unsigned count = 0U;
    for (char *token = strtok_r(line, " \t", &save); token; token = strtok_r(NULL, " \t", &save))
    {
        if (count == 6U) { snprintf(response, capacity, "ERR too many arguments\r\n"); return true; }
        tokens[count++] = token;
    }
    response[0] = '\0';
    if (strcmp(tokens[0], "vofa") == 0)
    {
        if (count == 2U && strcmp(tokens[1], "off") == 0) { view = 0U; }
        else if (count == 3U && strcmp(tokens[1], "on") == 0 && wheels(tokens[2]))
        {
            view = wheels(tokens[2]);
            last_wave_ms = osKernelGetTickCount();
        }
        else if (count == 3U && strcmp(tokens[1], "on") == 0 && strcmp(tokens[2], "pos") == 0)
        {
            view = 4U;
            last_wave_ms = osKernelGetTickCount();
        }
        else if (count == 3U && strcmp(tokens[1], "period") == 0 &&
                 (!strcmp(tokens[2], "20") || !strcmp(tokens[2], "50") || !strcmp(tokens[2], "100")))
        {
            period_ms = (uint32_t)atoi(tokens[2]);
            last_wave_ms = osKernelGetTickCount();
        }
        else { snprintf(response, capacity, "ERR vofa on left/right/both/pos | off | period 20/50/100\r\n"); return true; }
        snprintf(response, capacity, "OK vofa view=%u period_ms=%lu\r\n", view, (unsigned long)period_ms);
        return true;
    }
    if (count == 2U && !strcmp(tokens[1], "get")) { status_text(response, capacity); return true; }
    if (count == 2U && !strcmp(tokens[1], "stop"))
    {
        wheel_io_stop();
        snprintf(response, capacity, "OK speed stop queued\r\n");
        return true;
    }
    if (count == 5U && (!strcmp(tokens[1], "cfg") || !strcmp(tokens[1], "set")))
    {
        const uint8_t which = wheels(tokens[2]);
        speed_param_t param;
        float value;
        uint32_t id = 0U;
        const bool live = !strcmp(tokens[1], "set");
        if ((which == 1U || which == 2U) && parameter(tokens[3], &param) && decimal(tokens[4], &value) &&
            wheel_io_speed_stage(which == 1U ? 0U : 1U, param, value, live, &id))
        {
            if (live) { snprintf(response, capacity, "OK queued id=%lu\r\n", (unsigned long)id); }
            else { snprintf(response, capacity, "OK staged\r\n"); }
        }
        else { snprintf(response, capacity, "ERR invalid parameter or queue busy\r\n"); }
        return true;
    }
    speed_event_t event = {0};
    bool valid = false;
    if (count == 3U && !strcmp(tokens[1], "apply"))
    {
        event.type = SPEED_APPLY;
        event.wheels = wheels(tokens[2]);
        valid = event.wheels != 0U;
    }
    else if (count == 4U && !strcmp(tokens[1], "target"))
    {
        event.type = SPEED_TARGET;
        const uint8_t which = wheels(tokens[2]);
        if (which == 1U || which == 2U)
        {
            event.wheels = which;
            valid = decimal(tokens[3], &event.target[which == 1U ? 0U : 1U]);
        }
        else
        {
            event.wheels = 3U;
            valid = decimal(tokens[2], &event.target[0]) && decimal(tokens[3], &event.target[1]);
        }
    }
    if (valid && wheel_io_speed_enqueue(&event))
    {
        snprintf(response, capacity, "OK queued id=%lu\r\n", (unsigned long)event.id);
    }
    else { snprintf(response, capacity, "ERR speed arguments, state, or queue busy\r\n"); }
    return true;
}

bool speed_commands_poll_result(char *response, size_t capacity)
{
    speed_result_t result;
    if (!wheel_io_speed_result(&result)) { return false; }
    char kp0[24], ki0[24], ramp0[24], kp1[24], ki1[24], ramp1[24];
    fixed(kp0, sizeof(kp0), result.config[0].kp);
    fixed(ki0, sizeof(ki0), result.config[0].ki);
    fixed(ramp0, sizeof(ramp0), result.config[0].ramp_rpm_s);
    fixed(kp1, sizeof(kp1), result.config[1].kp);
    fixed(ki1, sizeof(ki1), result.config[1].ki);
    fixed(ramp1, sizeof(ramp1), result.config[1].ramp_rpm_s);
    snprintf(response, capacity,
             "%s id=%lu type=%u ms=%lu wheels=%u left_kp=%s left_ki=%s left_limit=%d "
             "left_ramp=%s right_kp=%s right_ki=%s right_limit=%d right_ramp=%s\r\n",
             result.accepted ? "OK applied" : "ERR rejected", (unsigned long)result.id,
             result.type, (unsigned long)result.applied_ms, result.wheels, kp0, ki0,
             result.config[0].limit, ramp0, kp1, ki1, result.config[1].limit, ramp1);
    return true;
}

bool speed_commands_wave_due(uint32_t tick, char *response, size_t capacity)
{
    if (view == 0U || tick - last_wave_ms < period_ms) { return false; }
    if (tick - last_wave_ms >= 2U * period_ms) { dropped_waves += (tick - last_wave_ms) / period_ms - 1U; }
    last_wave_ms = tick;
    wheel_io_status_t s;
    char a[24], b[24], c[24], d[24], e[24], f[24];
    int written;
    if (view == 4U)
    {
        wheel_position_status_t p;
        wheel_io_position_snapshot(&p, &s);
        fixed(a, sizeof(a), p.target_rpm[0]);
        fixed(b, sizeof(b), s.speed[0].feedback_rpm);
        written = snprintf(response, capacity,
                 "pos:%lu,%ld,%" PRId64 ",%" PRId64 ",%s,%s,%d,%u\n",
                 (unsigned long)s.sample_ms, (long)p.target[0], p.actual[0], p.error[0],
                 a, b, s.motor.applied_left, p.state);
    }
    else if (view == 3U)
    {
        wheel_io_get_status(&s);
        fixed(a, sizeof(a), s.speed[0].effective_rpm);
        fixed(b, sizeof(b), s.speed[0].feedback_rpm);
        fixed(c, sizeof(c), s.speed[1].effective_rpm);
        fixed(d, sizeof(d), s.speed[1].feedback_rpm);
        fixed(e, sizeof(e), (float)s.dt_us / 1000.0f);
        written = snprintf(response, capacity, "speed:%lu,%s,%s,%d,%s,%s,%d,%s,%u\n",
                 (unsigned long)s.sample_ms, a, b, s.motor.applied_left, c, d,
                 s.motor.applied_right, e, s.fault ? s.stop_reason : 0U);
    }
    else
    {
        wheel_io_get_status(&s);
        const unsigned i = view == 1U ? 0U : 1U;
        fixed(a, sizeof(a), s.speed[i].effective_rpm);
        fixed(b, sizeof(b), s.speed[i].feedback_rpm);
        fixed(c, sizeof(c), s.speed[i].error_rpm);
        fixed(d, sizeof(d), s.speed[i].config.kp);
        fixed(e, sizeof(e), s.speed[i].config.ki);
        fixed(f, sizeof(f), 0.0f);
        written = snprintf(response, capacity, "speed:%lu,%s,%s,%s,%d,%s,%s,%s,%ld\n",
                 (unsigned long)s.sample_ms, a, b, c,
                 i == 0U ? s.motor.applied_left : s.motor.applied_right, d, e, f,
                 (long)(i == 0U ? s.encoder.delta_left : s.encoder.delta_right));
    }
    if (written < 0 || (size_t)written >= capacity)
    {
        dropped_waves++;
        return false;
    }
    return true;
}

void speed_commands_wave_dropped(void) { dropped_waves++; }
bool speed_commands_result_pending(void) { return wheel_io_speed_result_pending(); }
