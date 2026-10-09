#include "rudder_control.h"

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

// 夹取
void grab_more_posture(void){
    rudder_control(2, 12);
}

// 张开
void open_more_posture(void){
    rudder_control(2, 12);
}

// 夹取
void grab_one_posture(void){
    rudder_control(1, 12);
}

// 张开
void open_one_posture(void){
    rudder_control(1, 12);
}

// 头部直立
void find_all_posture(void){
    rudder_control(3, 13);
}

// 寻找姿态
void find_posture(void){
    rudder_control(3, 150);
}

// 确认姿态
void confirm_posture(void){
    rudder_control(3, 13);
}

void rudder_task(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t TASK_PERIOD = pdMS_TO_TICKS(10); // 10ms周期
    rudder_init();
    while (1) {
//        grab_more_posture();
//        open_more_posture();
//        grab_one_posture();
//        open_one_posture();
//        find_all_posture();
//        find_posture();
//        confirm_posture();
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
