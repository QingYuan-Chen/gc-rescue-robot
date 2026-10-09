#include "sin_generate.h"

TaskHandle_t sin_task_handler;
#define PI 3.1415926f
																		//最大速度   角度跨度
struct sin_param sin_use_motor={0,0,60,0.15*10};

void sin_task(void *pvParameters){
	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();   //获取系统节拍
    uint8_t motor_idx = (uint8_t)(uint32_t)pvParameters;
	while(1){
        switch(motor_idx){
            case 1:
                motor_L0.target = sin_generator(&sin_use_motor);		//产生sin波形
                incremental_PID(&motor_L0,&L0_param);
                motor_set_pwm(1,(int32_t)motor_L0.output);
                break;
            case 2:
                motor_R0.target = sin_generator(&sin_use_motor);		//产生sin波形
                incremental_PID(&motor_R0,&R0_param);
                motor_set_pwm(2,(int32_t)motor_R0.output);
                break;
            case 3:
                motor_L1.target = sin_generator(&sin_use_motor);		//产生sin波形
                incremental_PID(&motor_L1,&L1_param);
                motor_set_pwm(3,(int32_t)motor_L1.output);
                break;
            case 4:
                motor_R1.target = sin_generator(&sin_use_motor);		//产生sin波形
                incremental_PID(&motor_R1,&R1_param);
                motor_set_pwm(4,(int32_t)motor_R1.output);
                break;
        }
        vTaskDelayUntil(&xLastWakeTime, (10/portTICK_RATE_MS));//绝对休眠10ms // INCLUDE_vTaskDelayUntil 1
	}
}

//主任务创建                                                                                                                                                                                
void sin_task_create(void){
	  xTaskCreate((TaskFunction_t ) sin_task,//任务函数
	               (const char *)"sin_task",	  //任务名字
								 (uint32_t) sin_task_size,    //任务堆栈大小
								 (void* )sin_motor_idx,                  //传递给任务参数的指针参数
								 (UBaseType_t) sin_task_priority, //任务的优先级
								(TaskHandle_t *)&sin_task_handler ); //任务句柄
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
