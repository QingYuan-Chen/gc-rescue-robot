#ifndef RESCUE_COMMON_H
#define RESCUE_COMMON_H

#include "main.h"
#include "gpio.h"
#include "tim.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"

#define sin_motor_idx 0

#define no_target 0
#define toward_target 1
#define disrupt_bolls 2

struct Vision_Manager{
    uint8_t team;
    uint8_t mode;
    uint8_t have_danger;
    uint8_t have_interrupt;
    uint8_t bolls_num;
    uint8_t x;
    uint8_t y;
};

struct Speed_Manager{
    float L0_speed;
    float R0_speed;
    float L1_speed;
    float R1_speed;
    float Car_speed;
};

struct Angle_Manager{
    float yaw;
};

struct MY_Car{
    struct Vision_Manager vision_manager;
    struct Speed_Manager speed_manager;
    struct Angle_Manager angle_manager;
    uint8_t track_mode;
    uint8_t rudder_mode;
};

volatile extern struct MY_Car my_car;

#endif //RESCUE_COMMON_H
