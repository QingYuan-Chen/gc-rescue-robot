#ifndef ROBOT_WHEEL_IO_H
#define ROBOT_WHEEL_IO_H

#include "bsp/encoder.h"
#include "bsp/motor.h"
#include "control/wheel_speed.h"

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
    wheel_speed_t speed[2];
    float target_rpm[2];
    uint32_t dt_us;
    uint8_t selected;
    uint8_t stop_reason;
    bool session;
    bool session_claimed;
    bool stop_pending;
    bool speed_running;
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

/* 直接设置左右轮闭环目标 RPM，下一控制周期生效；非零目标会持续保持。 */
bool wheel_io_speed_target(float left_rpm, float right_rpm);

/* 底盘层提交左右轮目标 RPM；返回 false 表示当前被其他控制源占用。 */
bool wheel_io_chassis_set(float left_rpm, float right_rpm);
bool wheel_io_chassis_claimed(void);

#endif
