#ifndef RESCUE_CAR_TASK_H
#define RESCUE_CAR_TASK_H

#include "common.h"
#include "maixcam.h"
#include "rudder_control.h"
#include "motor.h"
#include "sin_generate.h"
#include "grab_bolls.h"
#include "rudder_control.h"

extern TaskHandle_t Start_handler ; //定义开始任务句柄
void Start_task(void *pvParameters);//声明任务函数
#define Start_size  1024   //任务堆栈大小
#define Start_task_priority 32  //任务优先级


void Start_task_create(void);
void user_init(void);
void GET_free_RAM(TaskHandle_t xTask);

#endif //RESCUE_CAR_TASK_H
