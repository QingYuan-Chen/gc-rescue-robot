#ifndef Grab_Bolls_H
#define Grab_Bolls_H
#include "common.h"
#include "rudder_control.h"
#include "maixcam.h"
#include "motor.h"

#define NO_TARGET_THRESHOLD 5
#define TURN_AROUND_THRESHOLD 500

typedef enum {
    GRAB_WAIT_TEAM,        // 1. 等待队伍确认
    GRAB_SEARCH_BALL,      // 2. 寻找目标球
    GRAB_GO_TO_BALL,       // 3. 追踪靠近目标球
    GRAB_DISRUPT_BALL,     //撞干扰球
    GRAB_EXEC_GRAB,        // 4. 执行夹球动作
    GRAB_BACK_BASE,        // 5. 送回基地
    GRAB_FINISH            // 6. 任务完成
} GrabState;

extern TaskHandle_t grab_handler; 				//定义开始任务句柄
void grab_task(void *pvParameters);//声明任务函数
#define grab_size  (1024*2)			//任务堆栈大小
#define grab_task_priority 10 			//任务优先级
void grab_task_create(void);

#endif
