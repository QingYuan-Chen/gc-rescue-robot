#ifndef RESCUE_MOTOR_H
#define RESCUE_MOTOR_H

#include "common.h"
#include "pid.h"
#include "my_delay.h"
#include "rudder_control.h"

typedef enum {
    MOTOR_STOP,           // 1. 停止模式
    MOTOR_SEARCH_BALL,    // 2. 寻球模式
    MOTOR_TRACK_TARGET,   // 3. 追踪目标
    MOTOR_DISRUPT_BALL,   // 4. 撞干扰球
    MOTOR_FORWARD,
    MOTOR_BACKWARD        // 5. 后退
} MotorState;

void motor_init(void);
void pid_init(void);
void encoder_init(void);
void motor_set_pwm(uint8_t motor_idx, int32_t pid_out);
void motor_set_state(MotorState state);

extern TaskHandle_t motor_handler; 				//定义开始任务句柄
void motor_task(void *pvParameters);//声明任务函数
#define motor_size  (1024*2)			//任务堆栈大小
#define motor_task_priority 10 			//任务优先级
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

// G自平衡  ,T转弯    陀螺仪PID
extern struct P_pid_obj gyroT_pid;
extern struct P_pid_obj gyroG_pid;
extern struct PID_param gyroT_pid_param, gyroG_pid_param;

#endif //RESCUE_MOTOR_H
