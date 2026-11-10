#include "wheel_io.h"

#include "config/wheel_config.h"

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "task.h"

typedef struct
{
    int16_t left;
    int16_t right;
    uint32_t issued_ms;
    uint32_t duration_ms;
} drive_request_t;

/* 请求由调试任务写入、车轮任务读取；published 只由车轮任务更新。
 * 两个任务通过临界区交换完整结构，不能仅用 volatile 代替同步。
 */
static drive_request_t request;
static bool stop_pending;
static wheel_io_status_t published;
static uint32_t previous_ms;

bool wheel_io_init(uint32_t now_ms)
{
    request = (drive_request_t){0};
    published = (wheel_io_status_t){0};
    stop_pending = false;
    previous_ms = now_ms;
    /* 调度器启动后仅由高优先级车轮任务初始化一次，调试任务尚未获得运行。 */
    motor_init(now_ms);
    if (!encoder_init())
    {
        return false;
    }
    published.ready = true;
    return true;
}

bool wheel_io_request(int16_t left, int16_t right, uint32_t duration_ms)
{
    if (left < -WHEEL_COMMAND_MAX_DUTY || left > WHEEL_COMMAND_MAX_DUTY ||
        right < -WHEEL_COMMAND_MAX_DUTY || right > WHEEL_COMMAND_MAX_DUTY ||
        duration_ms < WHEEL_SAMPLE_PERIOD_MS || duration_ms > WHEEL_COMMAND_MAX_DURATION_MS)
    {
        return false;
    }
    taskENTER_CRITICAL();
    const bool accepted = published.ready && !published.fault && !stop_pending;
    if (accepted)
    {
        /* 单个请求总是定义双轮；新请求覆盖旧请求，时长从接收时开始计。 */
        request = (drive_request_t){left, right, osKernelGetTickCount(), duration_ms};
    }
    taskEXIT_CRITICAL();
    return accepted;
}

void wheel_io_stop(void)
{
    taskENTER_CRITICAL();
    /* 取消尚未生效的请求，并阻止停车被本周期后到的运动请求覆盖。 */
    request = (drive_request_t){0};
    stop_pending = true;
    taskEXIT_CRITICAL();
}

void wheel_io_step(uint32_t now_ms, bool delay_error)
{
    /* 唯一输出任务的固定周期入口。当前优先级高于调试任务，且下方不阻塞；
     * 清除 stop_pending 后，该周期会先完成停车再让调试任务处理下一条命令。
     */
    drive_request_t command;
    bool stopping;
    taskENTER_CRITICAL();
    command = request;
    stopping = stop_pending;
    stop_pending = false;
    taskEXIT_CRITICAL();

    wheel_io_status_t next = published;
    next.sample_ms = now_ms;
    next.interval_ms = now_ms - previous_ms;
    previous_ms = now_ms;
    /* 故障锁定直到复位；这里只检测任务实际得到执行后的迟到，不是硬件看门狗。 */
    next.fault |= delay_error || next.interval_ms > 2U * WHEEL_SAMPLE_PERIOD_MS;
    /* 无符号差值允许正常的 32 位 tick 回绕；过期、故障或停车统一交付零请求。 */
    const uint32_t age = now_ms - command.issued_ms;
    const bool active = !stopping && !next.fault && age < command.duration_ms;
    next.remaining_ms = active ? command.duration_ms - age : 0U;
    next.requested_left = active ? command.left : 0;
    next.requested_right = active ? command.right : 0;
    if (!motor_update(next.requested_left, next.requested_right, now_ms))
    {
        next.fault = true;
        next.requested_left = 0;
        next.requested_right = 0;
        next.remaining_ms = 0U;
    }
    /* 请求值与实际驱动阶段分别发布：唤醒中可以有请求，但实际 PWM 仍为零。 */
    motor_get_status(&next.motor);
    encoder_sample(&next.encoder);
    taskENTER_CRITICAL();
    published = next;
    taskEXIT_CRITICAL();
}

void wheel_io_get_status(wheel_io_status_t *status)
{
    taskENTER_CRITICAL();
    *status = published;
    taskEXIT_CRITICAL();
}
