#ifndef RESCUE_CAR_TASK_H
#define RESCUE_CAR_TASK_H

#include "common.h"
#include "maixcam.h"
#include "rudder_control.h"
#include "motor.h"
#include "sin_generate.h"
#include "grab_bolls.h"
#include "rudder_control.h"

extern TaskHandle_t Start_handler ; // 启动任务句柄
void Start_task(void *pvParameters);// 启动任务入口
#define Start_size  1024   // 任务栈深度，单位为 StackType_t 元素
#define Start_task_priority 32  // 任务优先级


void Start_task_create(void);
void user_init(void);
void GET_free_RAM(TaskHandle_t xTask);

#endif //RESCUE_CAR_TASK_H
