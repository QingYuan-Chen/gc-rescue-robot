#ifndef ROBOT_PID_H
#define ROBOT_PID_H

/* 离散参数与调用周期、输入输出单位绑定，不在算法内额外乘除采样时间。 */
typedef struct
{
    float kp;
    float ki;
    float kd;
    float differential_filter;
    float output_min;
    float output_max;
} pid_params_t;

/* 每个控制器独立保存状态，通过 step/reset 更新，不与其他轮子共享实例。 */
typedef struct
{
    float output;
    float error;
    float previous_error;
    float previous2_error;
    float measurement;
    float target;
} pid_incremental_state_t;

typedef struct
{
    float output;
    float error;
    float measurement;
    float previous_error;
    float integral;
    float previous_differential;
    float target;
} pid_positional_state_t;

void pid_incremental_reset(pid_incremental_state_t *state);
void pid_positional_reset(pid_positional_state_t *state);

/* 沿用原增量式算法：output_max 仅用于对称积分抑制，不截断内部累计输出。
 * output_min、differential_filter 不参与此算法，实际输出限幅由适配层负责。
 */
float pid_incremental_step(pid_incremental_state_t *state, const pid_params_t *params, float target,
                           float measurement);

/* 沿用原位置式算法：条件积分、差分滤波及非对称输出限幅，积分按调用次数累加。 */
float pid_positional_step(pid_positional_state_t *state, const pid_params_t *params, float target,
                          float measurement);

#endif
