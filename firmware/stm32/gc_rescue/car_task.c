#include "car_task.h"

volatile struct MY_Car my_car;

/* 初始化原工程的编码器、电机及视觉接收；任务创建由 Start_task 负责。 */
void user_init(void){
    encoder_init();
    motor_init();
    Maixcam_init();
}

TaskHandle_t Start_handler; // 启动任务句柄
void Start_task(void *pvParameters){
    taskENTER_CRITICAL();

    user_init();

    #if sin_work == 1
        sin_task_create();
    #endif

    #if grab_bolls_work ==1
        grab_task_create();
    #endif

    #if motor_work == 1
        motor_task_create();
    #endif

    #if rudder_work == 1
        rudder_task_create();
    #endif


    vTaskDelete(Start_handler); // 删除启动任务
    taskEXIT_CRITICAL();            // 退出临界区
}

/* 保留的调试入口：当前仅延时 500 个 RTOS 节拍，未使用 xTask 或读取任务栈。 */
void GET_free_RAM(TaskHandle_t xTask){
    vTaskDelay(500);
}

void Start_task_create(void){
    xTaskCreate((TaskFunction_t ) Start_task,// 任务函数
                (const char *)"Start_task",	  // 任务名称
                (uint32_t) Start_size,    // 任务栈深度，单位为 StackType_t 元素
                (void* )NULL,                  // 传给任务入口的参数
                (UBaseType_t) Start_task_priority, // 任务优先级
                (TaskHandle_t *)&Start_handler ); // 任务句柄
}

