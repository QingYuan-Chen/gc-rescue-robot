#include "wheel_pid.h"

#include "config/wheel_config.h"
#include "config/wheel_pid_config.h"

_Static_assert(WHEEL_PERIOD_MS == WHEEL_PID_REFERENCE_PERIOD_MS,
               "原 PID 参数要求 10 ms 调用周期，变更周期时须重新核对等效关系");

static const pid_params_t wheel_params = {
    .kp = WHEEL_PID_KP,
    .ki = WHEEL_PID_KI,
    .kd = WHEEL_PID_KD,
    .differential_filter = 0.0f,
    .output_min = 0.0f,
    .output_max = WHEEL_PID_OUTPUT_LIMIT_COUNTS,
};

int32_t wheel_pid_output_to_duty_permille(float output_counts)
{
    /* 只限制最终驱动请求，保留原增量式 PID 用于积分抑制的累计输出。 */
    if (output_counts > WHEEL_PID_OUTPUT_LIMIT_COUNTS)
    {
        output_counts = WHEEL_PID_OUTPUT_LIMIT_COUNTS;
    }
    else if (output_counts < -WHEEL_PID_OUTPUT_LIMIT_COUNTS)
    {
        output_counts = -WHEEL_PID_OUTPUT_LIMIT_COUNTS;
    }
    const int32_t ccr_counts = (int32_t)output_counts;
    return ccr_counts * 1000 / WHEEL_PID_REFERENCE_PWM_PERIOD_COUNTS;
}

int32_t wheel_pid_step_duty_permille(pid_incremental_state_t *state, float target_delta_counts,
                                     int32_t measured_delta_counts)
{
    const float output_counts = pid_incremental_step(state, &wheel_params, target_delta_counts,
                                                     (float)measured_delta_counts);
    return wheel_pid_output_to_duty_permille(output_counts);
}
