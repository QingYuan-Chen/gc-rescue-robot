#ifndef __SIN_GENERATE_H
#define __SIN_GENERATE_H

#include "common.h"
#include "motor.h"
#include <math.h>

struct sin_param {
	uint16_t time;	    // 波形生成调用次数，不是毫秒
	float actual_t;			// 当前相位，单位为度
	float gain;					// 波形幅值，沿用原 PID 目标计数尺度
	float angular_velocity;		// 每次调用的相位步进，单位为度
};

extern struct sin_param sin_use_motor;
float sin_generator(struct sin_param *param);

extern TaskHandle_t sin_handler; 				// 原正弦任务句柄声明，迁移时核对与实现的名称
void sin_task(void *pvParameters);// 正弦测试任务入口
#define sin_task_size  256   					// 任务栈深度，单位为 StackType_t 元素
#define sin_task_priority 1 			// 任务优先级
void sin_task_create(void);
#endif

