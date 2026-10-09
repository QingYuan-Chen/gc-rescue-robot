#ifndef __SIN_GENERATE_H
#define __SIN_GENERATE_H

#include "common.h"
#include "motor.h"
#include <math.h>

struct sin_param {
	uint16_t time;	    //时间
	float actual_t;			//角度
	float gain;					//最大速度
	float angular_velocity;		//角度跨度
};

extern struct sin_param sin_use_motor;
float sin_generator(struct sin_param *param);

extern TaskHandle_t sin_handler; 				//定义开始任务句柄
void sin_task(void *pvParameters);//声明任务函数
#define sin_task_size  256   					//任务堆栈大小
#define sin_task_priority 1 			//任务优先级
void sin_task_create(void);
#endif

