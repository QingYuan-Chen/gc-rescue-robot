#ifndef ROBOT_MOTOR_H
#define ROBOT_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MOTOR_OFF,
    MOTOR_WAKING,
    MOTOR_RUNNING
} motor_phase_t;

typedef struct
{
    /* 已接入 PWM 的有符号千分比；停止/唤醒阶段为零，不代表轮子已停稳。 */
    int16_t applied_left;
    int16_t applied_right;
    motor_phase_t phase_left;
    motor_phase_t phase_right;
} motor_status_t;

/* 唯一调用者为车轮周期任务，初始化和停车均切回 GPIO 双低，不启动 PWM。 */
void motor_init(uint32_t now_ms);
void motor_stop(uint32_t now_ms);

/* 单位为千分比，范围 -1000..1000；正号仅表示 OUT1->OUT2，车体方向待实测。 */
bool motor_update(int16_t left, int16_t right, uint32_t now_ms);
/* 零占空比保持制动仅用于已启用的速度环，普通零命令仍关闭输出。 */
bool motor_update_with_brake(int16_t left, int16_t right, bool brake_left, bool brake_right,
                             uint32_t now_ms);
void motor_get_status(motor_status_t *status);
void motor_guard_complete(uint32_t now_ms);
void motor_guard_begin(uint32_t now_ms);
void motor_guard_tick_isr(uint32_t now_ms);
bool motor_guard_fault(void);
void motor_force_outputs_low(void);

#endif
