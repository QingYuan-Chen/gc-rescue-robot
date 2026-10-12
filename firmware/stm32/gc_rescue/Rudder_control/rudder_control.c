#include "rudder_control.h"

// 以下保留旧机构的姿态命名与 CCR 参数，尚未迁移到当前 Servo1/Servo2 映射。

TaskHandle_t rudder_handler;

void rudder_init(void){
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_4);
}

void rudder_control(uint8_t rudder_idx, uint16_t ccr){
    switch (rudder_idx) {
        case 1:
            TIM1->CCR1 = ccr;
            break;
        case 2:
            TIM1->CCR2 = ccr;
            break;
        case 3:
            TIM1->CCR3 = ccr;
            break;
        default:
            break;
    }
}

void Set_Rudder_Mode(uint8_t mode){
    my_car.rudder_mode = mode;
}

// 原多球夹取姿态：通道 2，CCR 为 12。
void grab_more_posture(void){
    rudder_control(2, 12);
}

// 原多球释放姿态：当前与夹取使用相同的 CCR 值。
void open_more_posture(void){
    rudder_control(2, 12);
}

// 原单球夹取姿态：通道 1，CCR 为 12。
void grab_one_posture(void){
    rudder_control(1, 12);
}

// 原单球释放姿态：当前与夹取使用相同的 CCR 值。
void open_one_posture(void){
    rudder_control(1, 12);
}

// 原全范围寻找姿态：通道 3，CCR 为 13。
void find_all_posture(void){
    rudder_control(3, 13);
}

// 原寻找姿态：通道 3，CCR 为 150。
void find_posture(void){
    rudder_control(3, 150);
}

// 原确认姿态：通道 3，CCR 为 13。
void confirm_posture(void){
    rudder_control(3, 13);
}

void rudder_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t TASK_PERIOD = pdMS_TO_TICKS(10); // 预留的 10 ms 周期参数，当前循环尚未使用
    rudder_init();
    while (1) {
        // 当前参考任务未执行姿态动作或等待，调度方式在舵机模块迁移时处理。
    }

}

void rudder_task_create(void){
    xTaskCreate((TaskFunction_t ) rudder_task,
                (const char *)"rudder_task",
                (uint32_t) rudder_size,
                (void* )NULL,
                (UBaseType_t) rudder_task_priority,
                (TaskHandle_t *)&rudder_handler );
}
