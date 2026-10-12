#ifndef ROBOT_RUNTIME_MONITOR_H
#define ROBOT_RUNTIME_MONITOR_H

#include <stdint.h>

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

/* 在任务上下文持续观察基础时基和调度；本函数不返回。 */
void runtime_monitor_run(void);

/* 返回完整快照，不暴露模块内部状态；只在任务上下文调用。 */
void runtime_monitor_get_status(runtime_monitor_status_t *status);

#endif
