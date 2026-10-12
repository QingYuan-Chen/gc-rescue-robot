#ifndef rudder_control_H__
#define rudder_control_H__
#include "common.h"

#define one_grab 0x01
#define more_grab 0x02

void rudder_init(void);
void Set_Rudder_Mode(uint8_t mode);
void grab_more_posture(void);
void open_more_posture(void);
void grab_one_posture(void);
void open_one_posture(void);
void find_all_posture(void);
void find_posture(void);
void confirm_posture(void);

extern TaskHandle_t rudder_handler; 				// 舵机任务句柄
void rudder_task(void *pvParameters);// 舵机任务入口
#define rudder_size  (1024*2)			// 任务栈深度，单位为 StackType_t 元素
#define rudder_task_priority 10 			// 任务优先级
void rudder_task_create(void);

#endif
