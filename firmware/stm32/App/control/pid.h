#ifndef ROBOT_PID_H
#define ROBOT_PID_H

#include <stdbool.h>

/* Kp：输出单位/误差单位；Ki：输出单位/(误差单位·秒)；回算增益：1/秒。
 * 参数在初始化时确定，非负；启用积分时，回算增益须为正且 dt_s * gain <= 1。
 */
typedef struct
{
    float kp;
    float ki;
    float tracking_gain_per_s;
} pid_pi_params_t;

typedef struct
{
    float error;
    float proportional;
    float integral;
    float output;
    float next_integral;
} pid_pi_state_t;

void pid_pi_reset(pid_pi_state_t *state);

/* 每周期先计算请求，再用同周期最终采用的输出调用 track 一次。
 * integral 是本次输出使用的积分项；next_integral 是留给下一周期的状态。
 */
float pid_pi_prepare(pid_pi_state_t *state, const pid_pi_params_t *params, float target,
                     float measurement);

/* 仅限幅、换向等约束触发回算；普通整数 PWM 量化不回算，避免形成隐性死区。
 * Ki=0 时关闭积分和回算，保证纯 P 调试不会隐式产生积分输出。
 */
void pid_pi_track(pid_pi_state_t *state, const pid_pi_params_t *params, float dt_s,
                  float applied_output, bool constrained);

#endif
