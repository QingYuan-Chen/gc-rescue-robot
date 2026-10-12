#ifndef RESCUE_MOTOR_H
#define RESCUE_MOTOR_H

#include "common.h"
#include "pid.h"
#include "my_delay.h"
#include "rudder_control.h"

typedef enum {
    MOTOR_STOP,           // 停止模式
    MOTOR_SEARCH_BALL,    // 寻球模式
    MOTOR_TRACK_TARGET,   // 追踪目标
    MOTOR_DISRUPT_BALL,   // 撞干扰球
    MOTOR_FORWARD,        // 前进请求，当前任务未实现对应分支
    MOTOR_BACKWARD        // 后退
} MotorState;

void motor_init(void);
void pid_init(void);
void encoder_init(void);
// 原接口使用 1～4 号通道；pid_out 为带方向的 CCR 计数，不是占空比千分比。
void motor_set_pwm(uint8_t motor_idx, int32_t pid_out);
void motor_set_state(MotorState state);

extern TaskHandle_t motor_handler; 				// 电机任务句柄
void motor_task(void *pvParameters);// 电机任务入口
#define motor_size  (1024*2)			// 任务栈深度，单位为 StackType_t 元素
#define motor_task_priority 10 			// 任务优先级
void motor_task_create(void);

extern	struct PID_param L0_param;
extern	struct PID_param R0_param;
extern	struct PID_param L1_param;
extern	struct PID_param R1_param;

extern struct I_pid_obj motor_L0;
extern struct I_pid_obj motor_R0;
extern struct I_pid_obj motor_L1;
extern struct I_pid_obj motor_R1;

extern struct P_pid_obj Vision_x_pid_obj;
extern struct PID_param Vision_x_pid_param;

extern struct P_pid_obj Vision_w_pid_obj;
extern struct PID_param Vision_w_pid_param;

// 原工程预留的陀螺仪 PID 状态与参数，当前控制任务未使用。
extern struct P_pid_obj gyroT_pid;
extern struct P_pid_obj gyroG_pid;
extern struct PID_param gyroT_pid_param, gyroG_pid_param;

#endif //RESCUE_MOTOR_H
