#ifndef ROBOT_RUNTIME_MONITOR_H
#define ROBOT_RUNTIME_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    uint32_t system_clock_hz;
    uint32_t rtos_tick_hz;
    uint32_t period_ticks;
    uint32_t sample_count;
    uint32_t hal_tick_ms;
    uint32_t rtos_tick;
    uint32_t interval_ticks;
    uint32_t min_interval_ticks;
    uint32_t max_interval_ticks;
    uint32_t delay_error_count;
    uint32_t stack_free_bytes;
} runtime_monitor_status_t;

/* 由实际控制任务调用，不创建额外测试任务或独立循环。 */
void runtime_monitor_init(uint32_t period_ms);
void runtime_monitor_sample(uint32_t tick, bool delay_error);

/* 返回完整快照，不暴露模块内部状态；只在任务上下文调用。 */
void runtime_monitor_get_status(runtime_monitor_status_t *status);

#endif
