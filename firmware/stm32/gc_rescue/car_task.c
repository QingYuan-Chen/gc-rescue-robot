#include "car_task.h"

volatile struct MY_Car my_car;

/*****************************************************************************
函数名： user_init
函数功能：初始化外设，结构体等
*******************************************************************************/
void user_init(void){
    encoder_init();
    motor_init();
    Maixcam_init();
}

TaskHandle_t Start_handler; //定义开始任务句柄
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


    vTaskDelete(Start_handler); //删除开始任务
    taskEXIT_CRITICAL();            //退出临界区
}

/*****************************************************************************
函数名： GET_free_RAM
函数功能：获得任务的剩余堆栈大小 并且打印
形参：该任务的句柄      若传回NULL，则为该任务的堆栈
注意：INCLUDE_uxTaskGetStackHighWaterMark 1    才能使用
*******************************************************************************/
void GET_free_RAM(TaskHandle_t xTask){
//		printf("RAM = %d\r\n",(int32_t)uxTaskGetStackHighWaterMark(xTask));
    vTaskDelay(500);
}

void Start_task_create(void){
    xTaskCreate((TaskFunction_t ) Start_task,//任务函数
                (const char *)"Start_task",	  //任务名字
                (uint32_t) Start_size,    //任务堆栈大小
                (void* )NULL,                  //传递给任务参数的指针参数
                (UBaseType_t) Start_task_priority, //任务的优先级
                (TaskHandle_t *)&Start_handler ); //任务句柄
}

