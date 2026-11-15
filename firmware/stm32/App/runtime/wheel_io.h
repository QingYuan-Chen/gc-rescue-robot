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
    uint32_t applied_id;
    uint32_t applied_ms;
    uint8_t selected;
    uint8_t stop_reason;
    bool session;
    bool session_claimed;
    bool stop_pending;
    bool speed_running;
} wheel_io_status_t;

typedef enum
{
    SPEED_TARGET, SPEED_APPLY
} speed_event_type_t;

typedef struct
{
    speed_event_type_t type;
    uint8_t wheels;
    float target[2];
    wheel_speed_config_t config[2];
    uint32_t id;
} speed_event_t;

typedef struct
{
    uint32_t id;
    uint32_t applied_ms;
    speed_event_type_t type;
    bool accepted;
    uint8_t wheels;
    wheel_speed_config_t config[2];
} speed_result_t;

typedef enum { SPEED_PARAM_KP, SPEED_PARAM_KI, SPEED_PARAM_LIMIT, SPEED_PARAM_RAMP } speed_param_t;

typedef enum
{
    POSITION_IDLE, POSITION_QUEUED, POSITION_RUNNING, POSITION_DONE,
    POSITION_STOPPED, POSITION_TIMEOUT, POSITION_OVERSHOOT, POSITION_FAULT
} wheel_position_state_t;

typedef struct
{
    uint32_t id, started_ms, elapsed_ms;
    wheel_position_state_t state;
    int32_t target[2];
    int64_t actual[2], error[2];
    float target_rpm[2];
    uint8_t corrections[2];
} wheel_position_status_t;

typedef struct
{
    float kp, ki, kd, max_rpm;
    int32_t tolerance_counts;
} wheel_position_config_t;

/* 唯一周期任务调用；init 仅在启动时调用一次，初始化编码器，电机保持关闭。 */
bool wheel_io_init(uint32_t now_ms);
void wheel_io_step(uint32_t now_ms, bool delay_error);

/* 仅任务上下文调用；left/right 为有符号千分比，受 wheel_config 的联调范围限制。
 * 单个限时请求同时定义左右轮，显式停车优先；返回 true 仅表示接收，不表示已输出。
 */
bool wheel_io_request(int16_t left, int16_t right, uint32_t duration_ms);
void wheel_io_stop(void);
void wheel_io_get_status(wheel_io_status_t *status);
bool wheel_io_speed_stage(unsigned side, speed_param_t parameter, float value, bool live,
                          uint32_t *id);
void wheel_io_speed_get_staged(wheel_speed_config_t config[2]);
bool wheel_io_speed_enqueue(speed_event_t *event);
bool wheel_io_speed_result(speed_result_t *result);
bool wheel_io_speed_result_pending(void);
bool wheel_io_position_move(int32_t left, int32_t right, uint32_t *id);
bool wheel_io_position_slider_left(int32_t target, uint32_t *id);
void wheel_io_position_get(wheel_position_status_t *status);
void wheel_io_position_snapshot(wheel_position_status_t *position_status,
                                wheel_io_status_t *wheel_status);
void wheel_io_position_get_config(wheel_position_config_t *config);
bool wheel_io_position_set_config(const wheel_position_config_t *config);

#endif
