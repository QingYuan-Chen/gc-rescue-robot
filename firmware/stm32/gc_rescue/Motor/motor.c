#include "motor.h"

float MAX_SPEED_rate = 0.8f;
int32_t MOTOR_PWM_MAX = 0;

static MotorState motor_state = MOTOR_STOP;
TaskHandle_t motor_handler;

struct I_pid_obj motor_L0 = {0,0,0,0,0,0}; //0前1后
struct I_pid_obj motor_R0 = {0,0,0,0,0,0};
struct I_pid_obj motor_L1 = {0,0,0,0,0,0};
struct I_pid_obj motor_R1 = {0,0,0,0,0,0};

struct PID_param L0_param = {0,0,0,0,0,0};
struct PID_param R0_param = {0,0,0,0,0,0};
struct PID_param L1_param = {0,0,0,0,0,0};
struct PID_param R1_param = {0,0,0,0,0,0};

struct P_pid_obj Vision_x_pid_obj = {0,0,0,0,0,0,0};
struct PID_param Vision_x_pid_param = {0,0,0,0,0,0};;

struct P_pid_obj Vision_y_pid_obj = {0,0,0,0,0,0,0};
struct PID_param Vision_y_pid_param = {0,0,0,0,0,0};;

struct P_pid_obj gyroT_pid = {0,0,0,0,0,0,0};
struct P_pid_obj gyroG_pid = {0,0,0,0,0,0,0};
struct PID_param gyroT_pid_param = {0,0,0,0,0,0};
struct PID_param gyroG_pid_param = {0,0,0,0,0,0};

void encoder_init(void){
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
    HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);
}

void motor_pid_clear(void){
    motor_L0 = (struct I_pid_obj){0,0,0,0,0,0};
    motor_R0 = (struct I_pid_obj){0,0,0,0,0,0};
    motor_L1 = (struct I_pid_obj){0,0,0,0,0,0};
    motor_R1 = (struct I_pid_obj){0,0,0,0,0,0};
}

void pid_init(void){

    MOTOR_PWM_MAX = (int32_t)(htim1.Instance->ARR * MAX_SPEED_rate);
    //增量式PID 电机内环 带抗积分饱和
    L0_param.kp = 1600.0f;
    L0_param.ki = 180.0f;
    L0_param.kd = 0.0f;
    L0_param.outputMax = MOTOR_PWM_MAX;

    R0_param.kp = 1600.0f;
    R0_param.ki = 180.0f;
    R0_param.kd = 0.0f;
    R0_param.outputMax = MOTOR_PWM_MAX;

    L1_param.kp = 1600.0f;
    L1_param.ki = 180.0f;
    L1_param.kd = 0.0f;
    L1_param.outputMax = MOTOR_PWM_MAX;

    R1_param.kp = 1600.0f;
    R1_param.ki = 180.0f;
    R1_param.kd = 0.0f;
    R1_param.outputMax = MOTOR_PWM_MAX;

    //位置PID 外环 带微分项低通滤波
    //自平衡
    gyroG_pid_param.kp=1;
    gyroG_pid_param.ki=0;
    gyroG_pid_param.kd=0;
    gyroG_pid_param.outputMax = 500;
    gyroG_pid_param.outputMin = -gyroG_pid_param.outputMax;
    gyroG_pid_param.differential_filterK =0.5f;

    //左右
    Vision_x_pid_param.kp=1.25f;
    Vision_x_pid_param.ki=0;
    Vision_x_pid_param.kd=0.2f;
    Vision_x_pid_param.outputMax = 60;
    Vision_x_pid_param.outputMin = -60;
    Vision_x_pid_param.differential_filterK =0.5f;

    //前后
    Vision_y_pid_param.kp=0.6f;
    Vision_y_pid_param.ki=0;
    Vision_y_pid_param.kd=0.3f;
    Vision_y_pid_param.outputMax = 60;
    Vision_y_pid_param.outputMin = -60;
    Vision_y_pid_param.differential_filterK =0.5f;

    //原地旋转
    gyroT_pid_param.kp = 0.85f/3;
    gyroT_pid_param.ki = 0;
    gyroT_pid_param.kd = 0.25f;
    gyroT_pid_param.differential_filterK = 0.5f;
    gyroT_pid_param.outputMax = 5000;
    gyroT_pid_param.outputMin = -5000;

    motor_pid_clear();
}

void motor_init(void){
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_4);
    pid_init();
}

void get_encoder(void){
    motor_L0.measure = (float)(int16_t)TIM3->CNT;
    motor_R0.measure = (float)(int16_t)TIM2->CNT;
    motor_L1.measure = (float)-(int16_t)TIM4->CNT;
    motor_R1.measure = (float)-(int16_t)TIM8->CNT;

	TIM3->CNT = 0;
    TIM2->CNT = 0;
    TIM4->CNT = 0;
    TIM8->CNT = 0;
}

void motor_set_direction(uint8_t motor_idx, uint8_t motor_direction){
    switch(motor_idx){
        case 1:
            HAL_GPIO_WritePin(GPIOE,GPIO_PIN_12,motor_direction);
            HAL_GPIO_WritePin(GPIOB,GPIO_PIN_10,!motor_direction);
            break;
        case 2:
            HAL_GPIO_WritePin(GPIOE,GPIO_PIN_10,motor_direction);
            HAL_GPIO_WritePin(GPIOE,GPIO_PIN_8,!motor_direction);
            break;
        case 3:
            HAL_GPIO_WritePin(GPIOD,GPIO_PIN_3,motor_direction);
            HAL_GPIO_WritePin(GPIOD,GPIO_PIN_4,!motor_direction);
            break;
        case 4:
            HAL_GPIO_WritePin(GPIOD,GPIO_PIN_2,motor_direction);
            HAL_GPIO_WritePin(GPIOD,GPIO_PIN_1,!motor_direction);
            break;
        default:
            break;
    }
}

void motor_set_pwm(uint8_t motor_idx, int32_t pid_out){
    int32_t ccr = 0;
    if (pid_out >= 0){
        if (pid_out > MOTOR_PWM_MAX)
            ccr = MOTOR_PWM_MAX;
        else
            ccr = pid_out;
        switch (motor_idx){
            case 1:
                TIM1->CCR1 = ccr;
                motor_set_direction(1,1);
                break;
            case 2:
                TIM1->CCR2 = ccr;
                motor_set_direction(2,1);
                break;
            case 3:
                TIM1->CCR3 = ccr;
                motor_set_direction(3,1);
                break;
            case 4:
                TIM1->CCR4 = ccr;
                motor_set_direction(4,1);
                break;
            default:
                break;
        }
    }
    else{
        if (pid_out < -MOTOR_PWM_MAX)
            ccr = MOTOR_PWM_MAX;
        else
            ccr = -pid_out;
        switch (motor_idx){
            case 1:
                motor_set_direction(1,0);
                TIM1->CCR1 = ccr;
                break;
            case 2:
                motor_set_direction(2,0);
                TIM1->CCR2 = ccr;
                break;
            case 3:
                motor_set_direction(3,0);
                TIM1->CCR3 = ccr;
                break;
            case 4:
                motor_set_direction(4,0);
                TIM1->CCR4 = ccr;
                break;
            default:
                break;
        }
    }
}

void motor_task(void *pvParameters){
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t PERIOD = pdMS_TO_TICKS(10);

    while(1){
        get_encoder();
        float speed =  (motor_L0.measure+motor_R0.measure)*1.05f/2;

        taskENTER_CRITICAL();
        uint8_t vision_mode = my_car.vision_manager.mode;
        taskEXIT_CRITICAL();

        switch(motor_state){
            case MOTOR_STOP:
                motor_set_pwm(1,0);
                motor_set_pwm(2,0);
                motor_set_pwm(3,0);
                motor_set_pwm(4,0);
                break;
            case MOTOR_SEARCH_BALL:
                motor_set_pwm(1,50);
                motor_set_pwm(2,-50);
                motor_set_pwm(3,50);
                motor_set_pwm(4,-50);
                break;
            case MOTOR_TRACK_TARGET:
                Vision_x_pid_obj.measure = my_car.vision_manager.x;
                Vision_y_pid_obj.measure = my_car.vision_manager.y;
                Vision_x_pid_obj.target = 0;
                Vision_y_pid_obj.target = 0;

                float x_speed = positional_PID(&Vision_x_pid_obj, &Vision_x_pid_param);
                float y_speed = positional_PID(&Vision_y_pid_obj, &Vision_y_pid_param);

                motor_L0.target =  speed - x_speed*(speed/50) + y_speed*(speed/50);
                motor_R0.target =  speed + x_speed*(speed/50) + y_speed*(speed/50);
                motor_L1.target =  speed - x_speed*(speed/50) + y_speed*(speed/50);
                motor_R1.target =  speed + x_speed*(speed/50) + y_speed*(speed/50);

                incremental_PID(&motor_L0,&L0_param);
                incremental_PID(&motor_R0,&R0_param);
                incremental_PID(&motor_L1,&L1_param);
                incremental_PID(&motor_R1,&R1_param);

                motor_set_pwm(1,(int32_t)motor_L0.output);
                motor_set_pwm(2,(int32_t)motor_R0.output);
                motor_set_pwm(3,(int32_t)motor_L1.output);
                motor_set_pwm(4,(int32_t)motor_R1.output);
                break;
            case MOTOR_DISRUPT_BALL:
                motor_set_pwm(1,50);
                motor_set_pwm(2,-50);
                motor_set_pwm(3,50);
                motor_set_pwm(4,-50);
                my_delay_ms(50);

                motor_set_pwm(1,-50);
                motor_set_pwm(2,50);
                motor_set_pwm(3,-50);
                motor_set_pwm(4,50);
                my_delay_ms(50);

                motor_set_pwm(1,0);
                motor_set_pwm(2,0);
                motor_set_pwm(3,0);
                motor_set_pwm(4,0);

                motor_set_state(MOTOR_STOP);
                break;
            case MOTOR_BACKWARD:
                motor_set_pwm(1,-50);
                motor_set_pwm(2,-50);
                motor_set_pwm(3,-50);
                motor_set_pwm(4,-50);
                my_delay_ms(50);
                motor_set_state(MOTOR_STOP);
                break;
            default:
                motor_state = MOTOR_STOP;
                break;
        }

        vTaskDelayUntil(&xLastWakeTime, PERIOD);
    }
}

void motor_set_state(MotorState state){
    taskENTER_CRITICAL();
    motor_state = state;
    taskEXIT_CRITICAL();
}

void motor_task_create(void){
    xTaskCreate((TaskFunction_t ) motor_task,//任务函数
                (const char *)"motor_task",	  //任务名字
                (uint32_t) motor_size,    //任务堆栈大小
                (void* )NULL,                  //传递给任务参数的指针参数
                (UBaseType_t) motor_task_priority, //任务的优先级
                (TaskHandle_t *)&motor_handler ); //任务句柄
}