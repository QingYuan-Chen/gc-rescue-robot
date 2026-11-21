#include "speed_commands.h"

#include "runtime/chassis.h"
#include "runtime/heading.h"
#include "runtime/wheel_io.h"
#include "bsp/debug_uart.h"
#include "config/chassis_config.h"
#include "config/wheel_config.h"
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
    if (strcmp(text, "left") == 0) { return 1U; }
    if (strcmp(text, "right") == 0) { return 2U; }
    if (strcmp(text, "both") == 0) { return 3U; }
    return 0U;
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
    wheel_io_get_status(&s);
    debug_uart_get_status(&uart);
    char target[2][24], ref[2][24], feedback[2][24], error[2][24], p[2][24], integral[2][24];
    char free_output[2][24], kp[24], ki[24], ramp[24];
    fixed(kp, sizeof(kp), WHEEL_SPEED_KP);
    fixed(ki, sizeof(ki), WHEEL_SPEED_KI);
    fixed(ramp, sizeof(ramp), WHEEL_SPEED_RAMP_RPM_S);
    for (unsigned i = 0; i < 2U; i++)
    {
        fixed(target[i], sizeof(target[i]), s.target_rpm[i]);
        fixed(ref[i], sizeof(ref[i]), s.speed[i].effective_rpm);
        fixed(feedback[i], sizeof(feedback[i]), s.speed[i].feedback_rpm);
        fixed(error[i], sizeof(error[i]), s.speed[i].error_rpm);
        fixed(p[i], sizeof(p[i]), s.speed[i].proportional);
        fixed(integral[i], sizeof(integral[i]), s.speed[i].integral);
        fixed(free_output[i], sizeof(free_output[i]), s.speed[i].unconstrained);
    }
    snprintf(response, capacity,
             "OK speed session=%u claimed=%u stop_pending=%u running=%u selected=%u fault=%u reason=%u dt_us=%lu "
             "dropped=%lu\r\n"
             "OK left target=%s ref=%s rpm=%s error=%s p=%s i=%s free=%s req=%d out=%d "
             "phase=%u saturated=%u delta=%ld total=%" PRId64 " kp=%s ki=%s limit=%d ramp=%s\r\n"
             "OK right target=%s ref=%s rpm=%s error=%s p=%s i=%s free=%s req=%d out=%d "
             "phase=%u saturated=%u delta=%ld total=%" PRId64 " kp=%s ki=%s limit=%d ramp=%s\r\n",
             s.session, s.session_claimed, s.stop_pending, s.speed_running, s.selected, s.fault, s.stop_reason,
             (unsigned long)s.dt_us,
             (unsigned long)(dropped_waves + uart.sample_cancelled_count),
             target[0], ref[0], feedback[0], error[0], p[0], integral[0], free_output[0],
             s.speed[0].requested, s.motor.applied_left, s.speed[0].phase, s.speed[0].saturated,
             (long)s.encoder.delta_left, s.encoder.total_left, kp, ki, WHEEL_COMMAND_MAX_DUTY, ramp,
             target[1], ref[1], feedback[1], error[1], p[1], integral[1], free_output[1],
             s.speed[1].requested, s.motor.applied_right, s.speed[1].phase, s.speed[1].saturated,
             (long)s.encoder.delta_right, s.encoder.total_right, kp, ki, WHEEL_COMMAND_MAX_DUTY, ramp);
}

bool speed_commands_execute(char *line, char *response, size_t capacity)
{
    if (strncmp(line, "speed ", 6U) != 0 && strncmp(line, "vofa ", 5U) != 0) { return false; }
    /* VOFA+ 未绑定命令的滑块发送“名称:数值”，只转换已定义的滑块命令。 */
    char *colon = strchr(line, ':');
    if (colon != NULL)
    {
        static const char *const sliders[] = {"speed target left", "speed target right"};
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
        else if (count == 3U && strcmp(tokens[1], "on") == 0 &&
                 strcmp(tokens[2], "chassis") == 0)
        {
            view = 4U;
            last_wave_ms = osKernelGetTickCount();
        }
        else if (count == 3U && strcmp(tokens[1], "on") == 0 && strcmp(tokens[2], "turn") == 0)
        {
            view = 5U;
            last_wave_ms = osKernelGetTickCount();
        }
        else if (count == 3U && strcmp(tokens[1], "period") == 0 &&
                 (!strcmp(tokens[2], "20") || !strcmp(tokens[2], "50") || !strcmp(tokens[2], "100")))
        {
            period_ms = (uint32_t)atoi(tokens[2]);
            last_wave_ms = osKernelGetTickCount();
        }
        else { snprintf(response, capacity, "ERR vofa on left/right/both/chassis/turn | off | period 20/50/100\r\n"); return true; }
        snprintf(response, capacity, "OK vofa view=%u period_ms=%lu\r\n", view, (unsigned long)period_ms);
        return true;
    }
    if (count == 2U && !strcmp(tokens[1], "get")) { status_text(response, capacity); return true; }
    if (count == 2U && !strcmp(tokens[1], "stop"))
    {
        wheel_io_stop();
        snprintf(response, capacity, "OK speed stop\r\n");
        return true;
    }
    if (count == 4U && !strcmp(tokens[1], "target"))
    {
        /* 单轮命令保留另一轮当前目标，避免误停另一侧。 */
        wheel_io_status_t current;
        wheel_io_get_status(&current);
        const uint8_t which = wheels(tokens[2]);
        float left = current.target_rpm[0];
        float right = current.target_rpm[1];
        bool valid;
        if (which == 1U) { valid = decimal(tokens[3], &left); }
        else if (which == 2U) { valid = decimal(tokens[3], &right); }
        else { valid = decimal(tokens[2], &left) && decimal(tokens[3], &right); }
        if (valid && wheel_io_speed_target(left, right))
        {
            snprintf(response, capacity, "OK speed target left_mrpm=%ld right_mrpm=%ld\r\n",
                     (long)(left * 1000.0f), (long)(right * 1000.0f));
        }
        else { snprintf(response, capacity, "ERR speed target range or state\r\n"); }
        return true;
    }
    snprintf(response, capacity, "ERR unknown speed command\r\n");
    return true;
}

bool speed_commands_wave_due(uint32_t tick, char *response, size_t capacity)
{
    if (view == 0U || tick - last_wave_ms < period_ms) { return false; }
    if (tick - last_wave_ms >= 2U * period_ms) { dropped_waves += (tick - last_wave_ms) / period_ms - 1U; }
    last_wave_ms = tick;
    wheel_io_status_t s;
    char a[24], b[24], c[24], d[24], e[24], f[24], g[24], h[24];
    if (view == 4U)
    {
        chassis_status_t chassis;
        heading_status_t heading;
        chassis_get_status(&chassis);
        heading_get_status(&heading);
        wheel_io_get_status(&s);
        const float mm_per_rpm = 3.14159265358979323846f * CHASSIS_WHEEL_DIAMETER_MM / 60.0f;
        const float encoder_omega_deg_s =
            ((s.speed[1].feedback_rpm - s.speed[0].feedback_rpm) * mm_per_rpm /
             CHASSIS_TRACK_WIDTH_MM) * 57.2957795f;
        fixed(a, sizeof(a), chassis.v_mm_s);
        fixed(b, sizeof(b), chassis.omega_deg_s);
        fixed(c, sizeof(c), chassis.left_rpm);
        fixed(d, sizeof(d), s.speed[0].feedback_rpm);
        fixed(e, sizeof(e), chassis.right_rpm);
        fixed(f, sizeof(f), s.speed[1].feedback_rpm);
        fixed(g, sizeof(g), heading.yaw_rate_rad_s * 57.2957795f);
        fixed(h, sizeof(h), encoder_omega_deg_s);
        snprintf(response, capacity,
                 "chassis:%lu,%s,%s,%s,%s,%s,%s,%s,%s,%u\n",
                 (unsigned long)s.sample_ms, a, b, c, d, e, f, g, h,
                 chassis.saturated ? 1U : 0U);
    }
    else if (view == 5U)
    {
        chassis_status_t chassis;
        heading_status_t heading;
        chassis_get_status(&chassis);
        heading_get_status(&heading);
        wheel_io_get_status(&s);
        fixed(a, sizeof(a), chassis.target_yaw_deg);
        fixed(b, sizeof(b), chassis.yaw_deg);
        fixed(c, sizeof(c), chassis.yaw_error_deg);
        fixed(d, sizeof(d), chassis.yaw_rate_dps);
        fixed(e, sizeof(e), chassis.omega_deg_s);
        fixed(f, sizeof(f), chassis.left_rpm);
        fixed(g, sizeof(g), chassis.right_rpm);
        snprintf(response, capacity,
                 "turn:%lu,%s,%s,%s,%s,%s,%s,%s,%u,%u,%lu\n",
                 (unsigned long)s.sample_ms, a, b, c, d, e, f, g,
                 (unsigned)chassis.mode, chassis.stop_reason,
                 (unsigned long)heading.age_ms);
    }
    else if (view == 3U)
    {
        wheel_io_get_status(&s);
        fixed(a, sizeof(a), s.speed[0].effective_rpm);
        fixed(b, sizeof(b), s.speed[0].feedback_rpm);
        fixed(c, sizeof(c), s.speed[1].effective_rpm);
        fixed(d, sizeof(d), s.speed[1].feedback_rpm);
        fixed(e, sizeof(e), (float)s.dt_us / 1000.0f);
        snprintf(response, capacity, "speed:%lu,%s,%s,%d,%s,%s,%d,%s,%u\n",
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
        fixed(d, sizeof(d), WHEEL_SPEED_KP);
        fixed(e, sizeof(e), WHEEL_SPEED_KI);
        fixed(f, sizeof(f), 0.0f);
        snprintf(response, capacity, "speed:%lu,%s,%s,%s,%d,%s,%s,%s,%ld\n",
                 (unsigned long)s.sample_ms, a, b, c,
                 i == 0U ? s.motor.applied_left : s.motor.applied_right, d, e, f,
                 (long)(i == 0U ? s.encoder.delta_left : s.encoder.delta_right));
    }
    return true;
}

void speed_commands_wave_dropped(void) { dropped_waves++; }
