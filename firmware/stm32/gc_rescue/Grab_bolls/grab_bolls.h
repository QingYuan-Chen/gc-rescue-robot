#ifndef Grab_Bolls_H
#define Grab_Bolls_H
#include "common.h"
#include "rudder_control.h"
#include "maixcam.h"
#include "motor.h"

#define NO_TARGET_THRESHOLD 5 // 连续丢失目标的任务轮次阈值
#define TURN_AROUND_THRESHOLD 500 // 无目标搜索的任务轮次阈值

typedef enum {
    GRAB_WAIT_TEAM,        // 等待队伍确认
    GRAB_SEARCH_BALL,      // 寻找目标球
    GRAB_GO_TO_BALL,       // 追踪靠近目标球
    GRAB_DISRUPT_BALL,     // 撞干扰球
    GRAB_EXEC_GRAB,        // 执行夹球动作
    GRAB_BACK_BASE,        // 送回基地
    GRAB_FINISH            // 任务完成
} GrabState;

extern TaskHandle_t grab_handler; 				// 抓取任务句柄
void grab_task(void *pvParameters);// 抓取任务入口
#define grab_size  (1024*2)			// 任务栈深度，单位为 StackType_t 元素
#define grab_task_priority 10 			// 任务优先级
void grab_task_create(void);

#endif
