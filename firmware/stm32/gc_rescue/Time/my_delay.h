#ifndef MY_DELAY_H
#define MY_DELAY_H

#include "common.h"

// 参数直接传给 osDelay，单位为 RTOS 节拍；当前 1000 Hz 配置下每节拍为 1 ms。
void my_delay_ms(uint32_t time);

#endif // MY_DELAY_H
