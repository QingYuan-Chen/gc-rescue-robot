#include "sin_generate.h"

TaskHandle_t sin_task_handler;
#define PI 3.1415926f
																		// 原测试幅值为 60，每次调用的相位步进为 1.5°。
struct sin_param sin_use_motor={0,0,60,0.15*10};

void sin_task(void *pvParameters){
	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();   // 获取系统节拍
    uint8_t motor_idx = (uint8_t)(uint32_t)pvParameters;
	while(1){
        switch(motor_idx){
            case 1:
                motor_L0.target = sin_generator(&sin_use_motor);		// 生成正弦目标值
                incremental_PID(&motor_L0,&L0_param);
                motor_set_pwm(1,(int32_t)motor_L0.output);
                break;
            case 2:
                motor_R0.target = sin_generator(&sin_use_motor);		// 生成正弦目标值
                incremental_PID(&motor_R0,&R0_param);
                motor_set_pwm(2,(int32_t)motor_R0.output);
                break;
            case 3:
                motor_L1.target = sin_generator(&sin_use_motor);		// 生成正弦目标值
                incremental_PID(&motor_L1,&L1_param);
                motor_set_pwm(3,(int32_t)motor_L1.output);
                break;
            case 4:
                motor_R1.target = sin_generator(&sin_use_motor);		// 生成正弦目标值
                incremental_PID(&motor_R1,&R1_param);
                motor_set_pwm(4,(int32_t)motor_R1.output);
                break;
        }
        vTaskDelayUntil(&xLastWakeTime, (10/portTICK_RATE_MS));// 以 10 ms 为调度周期，需启用 INCLUDE_vTaskDelayUntil。
	}
}

// 创建正弦测试任务
void sin_task_create(void){
	  xTaskCreate((TaskFunction_t ) sin_task,// 任务函数
	               (const char *)"sin_task",	  // 任务名称
								 (uint32_t) sin_task_size,    // 任务栈深度，单位为 StackType_t 元素
								 (void* )sin_motor_idx,                  // 通过入口参数传入原电机编号
								 (UBaseType_t) sin_task_priority, // 任务优先级
								(TaskHandle_t *)&sin_task_handler ); // 任务句柄
							 }


float sin_generator(struct sin_param *param)
{
	float output;
	
	param->actual_t = param->time * param->angular_velocity;
	
	output = param->gain * sin(param->actual_t * PI/180);
	
	++param->time;
	
	if (param->actual_t >= 360)
		param->time = 0;
	
	return output;
}
