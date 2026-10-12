#include "runtime_monitor.h"

#include "cmsis_os2.h"
#include "stm32h7xx_hal.h"

#include <stdint.h>

#define RUNTIME_MONITOR_PERIOD_MS 10U

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
} runtime_status_t;

/* 仅本任务写入；volatile 用于保留 SWD 观察数据，不承担任务间同步职责。 */
static volatile runtime_status_t runtime_status;

void runtime_monitor_run(void)
{
    const uint32_t tick_hz = osKernelGetTickFreq();
    const uint32_t period_ticks = RUNTIME_MONITOR_PERIOD_MS * tick_hz / 1000U;
    const osThreadId_t task = osThreadGetId();
    uint32_t previous_tick = osKernelGetTickCount();
    uint32_t next_tick = previous_tick;

    runtime_status.system_clock_hz = SystemCoreClock;
    runtime_status.rtos_tick_hz = tick_hz;
    runtime_status.period_ticks = period_ticks;
    runtime_status.min_interval_ticks = UINT32_MAX;

    for (;;)
    {
        next_tick += period_ticks;
        if (osDelayUntil(next_tick) != osOK)
        {
            /* 记录错过的调度点，并重新对齐，避免连续追赶造成忙循环。 */
            runtime_status.delay_error_count++;
            next_tick = osKernelGetTickCount();
        }

        const uint32_t current_tick = osKernelGetTickCount();
        const uint32_t interval_ticks = current_tick - previous_tick;
        previous_tick = current_tick;

        runtime_status.hal_tick_ms = HAL_GetTick();
        runtime_status.rtos_tick = current_tick;
        runtime_status.interval_ticks = interval_ticks;
        if (interval_ticks < runtime_status.min_interval_ticks)
        {
            runtime_status.min_interval_ticks = interval_ticks;
        }
        if (interval_ticks > runtime_status.max_interval_ticks)
        {
            runtime_status.max_interval_ticks = interval_ticks;
        }
        runtime_status.stack_free_bytes = osThreadGetStackSpace(task);
        runtime_status.sample_count++;
    }
}
