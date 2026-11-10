#ifndef ROBOT_WHEEL_IO_H
#define ROBOT_WHEEL_IO_H

#include "bsp/encoder.h"
#include "bsp/motor.h"

typedef struct
{
    bool ready;
    bool fault;
    /* RTOS 固定为 1 kHz，以下时间单位均为 ms。 */
    uint32_t sample_ms;
    uint32_t interval_ms;
    uint32_t remaining_ms;
    /* 本次周期采用的有效请求，单位为千分比；零不表示闭环零速保持。 */
    int16_t requested_left;
    int16_t requested_right;
    encoder_sample_t encoder;
    motor_status_t motor;
} wheel_io_status_t;

/* 唯一周期任务调用；init 仅在启动时调用一次，初始化编码器，电机保持关闭。 */
bool wheel_io_init(uint32_t now_ms);
void wheel_io_step(uint32_t now_ms, bool delay_error);

/* 仅任务上下文调用；left/right 为有符号千分比，受 wheel_config 的联调范围限制。
 * 单个限时请求同时定义左右轮，显式停车优先；返回 true 仅表示接收，不表示已输出。
 */
bool wheel_io_request(int16_t left, int16_t right, uint32_t duration_ms);
void wheel_io_stop(void);
void wheel_io_get_status(wheel_io_status_t *status);

#endif
