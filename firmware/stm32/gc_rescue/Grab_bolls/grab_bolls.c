#include "grab_bolls.h"
#include "my_delay.h"
#include "cmsis_os.h"

TaskHandle_t grab_handler;

static GrabState grab_state = GRAB_WAIT_TEAM;
static uint8_t no_target_cnt = 0;           // 跟踪过程中连续丢失目标的任务轮次
static uint16_t turn_around_cnt = 0;           // 搜索过程中无目标的累计任务轮次
static uint16_t have_grab_ball = 0;

void grab_task(void *pvParameters){
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t TASK_PERIOD = pdMS_TO_TICKS(10); // 标称调度周期为 10 ms

    while(1){
        taskENTER_CRITICAL();
        uint8_t now_team = my_car.vision_manager.team;
        uint8_t vision_mode = my_car.vision_manager.mode;
        uint8_t boll_x = my_car.vision_manager.x;
        uint8_t boll_y = my_car.vision_manager.y;
        uint8_t have_danger = my_car.vision_manager.have_danger;
        uint8_t bolls_num = my_car.vision_manager.bolls_num;
        uint8_t have_interrupt = my_car.vision_manager.have_interrupt;
        taskEXIT_CRITICAL();

        switch(grab_state){
            case GRAB_WAIT_TEAM:
                motor_set_state(MOTOR_STOP);
                if(now_team == 0x01 || now_team == 0x02){
                    grab_state = GRAB_SEARCH_BALL;
                }
                break;
            case GRAB_SEARCH_BALL:
                find_posture();
                if(vision_mode == no_target){
                    turn_around_cnt++;
                    if(turn_around_cnt >= TURN_AROUND_THRESHOLD){
                        motor_set_state(MOTOR_STOP);
                        find_all_posture();
                    }
                    motor_set_state(MOTOR_SEARCH_BALL);
                }
                else if(vision_mode == toward_target){
                    if(have_danger == 1 && !have_grab_ball){
                        Set_Vision_Mode(danger_mode);
                    }
                    else{
                        Set_Vision_Mode(normal_mode);
                    }
                    turn_around_cnt = 0;
                    grab_state = GRAB_GO_TO_BALL;
                }
                break;
            case GRAB_GO_TO_BALL:
                if(have_interrupt){
                    motor_set_state(MOTOR_STOP);
                    grab_state = GRAB_EXEC_GRAB;
                }
                if(boll_y < 120){
                    motor_set_state(MOTOR_STOP);
                    find_posture();
                }
                motor_set_state(MOTOR_TRACK_TARGET);
                if(boll_x > 20 && boll_x < 220 && boll_y < 60){
                    no_target_cnt = 0;
                    motor_set_state(MOTOR_STOP);
                    if(vision_mode == danger_mode || vision_mode == normal_mode){
                        grab_state = GRAB_EXEC_GRAB;
                    }
                    else if(vision_mode == base_mode){
                        grab_state = GRAB_BACK_BASE;
                    }
                }
                else if(vision_mode == no_target){
                    no_target_cnt++;
                    if(no_target_cnt >= NO_TARGET_THRESHOLD){
                        no_target_cnt = 0;
                        grab_state = GRAB_SEARCH_BALL;
                    }
                }
                else if(vision_mode == toward_target){
                    no_target_cnt = 0;
                }
                break;
            case GRAB_DISRUPT_BALL:
                motor_set_state(MOTOR_DISRUPT_BALL);
                my_delay_ms(20);
                grab_state = GRAB_GO_TO_BALL;
                break;
            case GRAB_EXEC_GRAB:
                if(vision_mode == danger_mode){
                    grab_one_posture();
                }
                else{
                    grab_more_posture();
                }
                my_delay_ms(20);
                confirm_posture();
                my_delay_ms(20);
                if(vision_mode == normal_mode){
                    if(have_danger == 0 && bolls_num < 3){
                        grab_state = GRAB_SEARCH_BALL;
                        have_grab_ball+=1;
                    }
                    else if(have_danger == 0 && bolls_num == 3){
                        Set_Vision_Mode(base_mode);
                        grab_state = GRAB_SEARCH_BALL;
                    }
                    else if(have_danger == 0 && bolls_num > 3){
                        grab_state = GRAB_DISRUPT_BALL;
                    }
                    else if(have_danger == 1){
                        grab_state = GRAB_DISRUPT_BALL;
                    }
                }
                else{
                    if(have_danger == 1 && bolls_num == 1){
                        Set_Vision_Mode(base_mode);
                        grab_state = GRAB_SEARCH_BALL;
                    }
                }
                break;
            case GRAB_BACK_BASE:
                motor_set_state(MOTOR_FORWARD);
                my_delay_ms(20);
                if(my_car.rudder_mode == one_grab){
                    open_one_posture();
                    my_delay_ms(20);
                }
                else{
                    open_more_posture();
                    my_delay_ms(20);
                }
                grab_state = GRAB_FINISH;
                break;
            case GRAB_FINISH:
                motor_set_state(MOTOR_BACKWARD);
                my_delay_ms(20);
                if(my_car.rudder_mode == more_grab){
                    grab_more_posture();
                    my_delay_ms(20);
                }
                have_grab_ball = 0;
                grab_state = GRAB_SEARCH_BALL;
                break;
            default:
                grab_state = GRAB_WAIT_TEAM;
                break;
        }

        // 以 10 ms 为调度基准；分支中的阻塞延时会延长实际执行间隔。
        vTaskDelayUntil(&xLastWakeTime, TASK_PERIOD);
    }
}

void grab_task_create(void){
    xTaskCreate((TaskFunction_t ) grab_task,
                (const char *)"grab_task",
                (uint32_t) grab_size,
                (void* )NULL,
                (UBaseType_t) grab_task_priority,
                (TaskHandle_t *)&grab_handler );
}