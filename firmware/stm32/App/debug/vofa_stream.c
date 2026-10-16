#include "vofa_stream.h"

#include "control/wheel_drive.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

typedef struct
{
    uint32_t elapsed_ms;
    int32_t target_milli_counts;
    int32_t delta_counts;
    int32_t duty_permille;
    float kp;
    float ki;
} vofa_sample_t;

/* 单一控制任务生产、单一调试任务消费；短临界区保护开关和环形队列。 */
static vofa_sample_t samples[VOFA_SAMPLE_CAPACITY];
static uint32_t head;
static uint32_t tail;
static uint32_t start_tick_ms;
static vofa_stream_status_t state;

void vofa_stream_start(uint32_t tick_ms)
{
    taskENTER_CRITICAL();
    if (!state.enabled)
    {
        head = tail = 0U;
        start_tick_ms = tick_ms;
        state = (vofa_stream_status_t){.enabled = true};
    }
    taskEXIT_CRITICAL();
}

void vofa_stream_stop(void)
{
    taskENTER_CRITICAL();
    state.enabled = false;
    state.queued = 0U;
    head = tail = 0U;
    taskEXIT_CRITICAL();
}

void vofa_stream_get_status(vofa_stream_status_t *status)
{
    taskENTER_CRITICAL();
    *status = state;
    taskEXIT_CRITICAL();
}

void vofa_stream_capture(void)
{
    taskENTER_CRITICAL();
    const bool enabled = state.enabled;
    taskEXIT_CRITICAL();
    if (!enabled)
    {
        return;
    }
    wheel_drive_status_t wheel;
    wheel_drive_get_status(&wheel);
    if (!wheel.ready)
    {
        return;
    }
    taskENTER_CRITICAL();
    if (state.enabled)
    {
        if (state.queued == VOFA_SAMPLE_CAPACITY)
        {
            /* 优先保留近期采样；丢样由计数和时间戳间隔体现，不阻塞控制。 */
            tail = (tail + 1U) % VOFA_SAMPLE_CAPACITY;
            state.queued--;
            state.dropped++;
        }
        samples[head] = (vofa_sample_t){
            .elapsed_ms = wheel.sample_tick_ms - start_tick_ms,
            .target_milli_counts = wheel.target_milli_counts[WHEEL_LEFT],
            .delta_counts = wheel.delta_counts[WHEEL_LEFT],
            .duty_permille = wheel.duty_permille[WHEEL_LEFT],
            .kp = wheel.pi[WHEEL_LEFT].params.kp,
            .ki = wheel.pi[WHEEL_LEFT].params.ki,
        };
        head = (head + 1U) % VOFA_SAMPLE_CAPACITY;
        state.queued++;
        state.captured++;
    }
    taskEXIT_CRITICAL();
}

size_t vofa_stream_format_next(char *line, size_t capacity)
{
    vofa_sample_t sample;
    taskENTER_CRITICAL();
    if (state.queued == 0U)
    {
        taskEXIT_CRITICAL();
        return 0U;
    }
    sample = samples[tail];
    tail = (tail + 1U) % VOFA_SAMPLE_CAPACITY;
    state.queued--;
    taskEXIT_CRITICAL();

    /* 只在后台格式化。时间先拆成整数秒和毫秒，避免 float32 绝对时间精度损失。
     * 误差始终为目标减反馈，停车时也可观察惯性反馈，不使用已清零的 PI 内部误差。
     */
    const int32_t error_milli = sample.target_milli_counts - sample.delta_counts * 1000;
    const int length = snprintf(
        line, capacity, "vofa:%lu.%03lu,%.3f,%ld,%.3f,%ld,%.3f,%.3f,0.000\r\n",
        (unsigned long)(sample.elapsed_ms / 1000U), (unsigned long)(sample.elapsed_ms % 1000U),
        (double)sample.target_milli_counts / 1000.0, (long)sample.delta_counts,
        (double)error_milli / 1000.0, (long)sample.duty_permille, (double)sample.kp,
        (double)sample.ki);
    if (length <= 0 || (size_t)length >= capacity)
    {
        taskENTER_CRITICAL();
        state.dropped++;
        taskEXIT_CRITICAL();
        return 0U;
    }
    return (size_t)length;
}

void vofa_stream_record_tx(bool success)
{
    taskENTER_CRITICAL();
    if (success)
    {
        state.sent++;
    }
    else
    {
        state.tx_errors++;
    }
    taskEXIT_CRITICAL();
}
