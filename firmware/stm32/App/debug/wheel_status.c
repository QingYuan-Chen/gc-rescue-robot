#include "wheel_status.h"

#include "config/wheel_config.h"
#include "control/wheel_drive.h"

#include <stdio.h>

void wheel_status_format(char *response, size_t capacity)
{
    static const char *const modes[] = {"stop", "open", "speed"};
    static const char *const stops[] = {"boot", "command", "expired", "period"};
    static const char *const rejections[] = {"none",         "invalid",      "not_ready",
                                             "stop_pending", "period_fault", "busy"};
    wheel_drive_status_t status;
    wheel_drive_get_status(&status);
    const int length = snprintf(
        response, capacity,
        "OK wheel tick_ms=%lu period_ms=%u interval_ms=%lu remaining_ms=%lu "
        "mode=%s source=%s controller=pi output_unit=permille ready=%u fault=%u "
        "transition=%u stop=%s reject=%s "
        "enable_left=%u enable_right=%u target_left_milli=%ld target_right_milli=%ld "
        "setpoint_left_milli=%ld setpoint_right_milli=%ld "
        "p_left=%.3f p_right=%.3f i_left=%.3f i_right=%.3f "
        "i_next_left=%.3f i_next_right=%.3f pi_left=%.3f pi_right=%.3f "
        "request_left=%ld request_right=%ld "
        "duty_left=%ld duty_right=%ld limited_left=%u limited_right=%u "
        "reverse_left=%u reverse_right=%u delta_left=%ld delta_right=%ld "
        "total_left=%lld total_right=%lld raw_left=%lu raw_right=%lu\r\n",
        (unsigned long)status.sample_tick_ms, WHEEL_PERIOD_MS, (unsigned long)status.interval_ms,
        (unsigned long)status.remaining_ms, modes[status.mode],
        status.mode == WHEEL_MODE_STOP ? "none" : "debug", (unsigned)status.ready,
        (unsigned)status.period_fault, (unsigned)status.transitioning,
        stops[status.last_stop_reason], rejections[status.last_reject_reason],
        (unsigned)status.enabled[WHEEL_LEFT], (unsigned)status.enabled[WHEEL_RIGHT],
        (long)status.target_milli_counts[WHEEL_LEFT], (long)status.target_milli_counts[WHEEL_RIGHT],
        (long)status.configured_target_milli_counts[WHEEL_LEFT],
        (long)status.configured_target_milli_counts[WHEEL_RIGHT],
        (double)status.pi[WHEEL_LEFT].proportional_permille,
        (double)status.pi[WHEEL_RIGHT].proportional_permille,
        (double)status.pi[WHEEL_LEFT].integral_permille,
        (double)status.pi[WHEEL_RIGHT].integral_permille,
        (double)status.pi[WHEEL_LEFT].next_integral_permille,
        (double)status.pi[WHEEL_RIGHT].next_integral_permille,
        (double)status.pi[WHEEL_LEFT].output_permille,
        (double)status.pi[WHEEL_RIGHT].output_permille,
        (long)status.requested_duty_permille[WHEEL_LEFT],
        (long)status.requested_duty_permille[WHEEL_RIGHT], (long)status.duty_permille[WHEEL_LEFT],
        (long)status.duty_permille[WHEEL_RIGHT], (unsigned)status.limited[WHEEL_LEFT],
        (unsigned)status.limited[WHEEL_RIGHT], (unsigned)status.reversing[WHEEL_LEFT],
        (unsigned)status.reversing[WHEEL_RIGHT], (long)status.delta_counts[WHEEL_LEFT],
        (long)status.delta_counts[WHEEL_RIGHT], (long long)status.total_counts[WHEEL_LEFT],
        (long long)status.total_counts[WHEEL_RIGHT], (unsigned long)status.raw_counts[WHEEL_LEFT],
        (unsigned long)status.raw_counts[WHEEL_RIGHT]);
    if (length < 0 || (size_t)length >= capacity)
    {
        snprintf(response, capacity, "ERR wheel status buffer\r\n");
    }
}
