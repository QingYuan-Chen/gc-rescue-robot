#include "runtime_monitor.h"

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "stm32h7xx_hal.h"
#include "task.h"

#include <stdint.h>

#define RUNTIME_MONITOR_PERIOD_MS 10U

/* volatile 仅保留 SWD 观察数据；任务间快照由临界区保护。 */
static volatile runtime_monitor_status_t runtime_status;

void runtime_monitor_get_status(runtime_monitor_status_t *status)
{
    taskENTER_CRITICAL();
    *status = runtime_status;
    taskEXIT_CRITICAL();
}

void runtime_monitor_run(void)
{
    const uint32_t tick_hz = osKernelGetTickFreq();
    const uint32_t period_ticks = RUNTIME_MONITOR_PERIOD_MS * tick_hz / 1000U;
    const osThreadId_t task = osThreadGetId();
    uint32_t previous_tick = osKernelGetTickCount();
    uint32_t next_tick = previous_tick;

    runtime_monitor_status_t sample = {
        .system_clock_hz = SystemCoreClock,
        .rtos_tick_hz = tick_hz,
        .period_ticks = period_ticks,
        .min_interval_ticks = UINT32_MAX,
    };
    taskENTER_CRITICAL();
    runtime_status = sample;
    taskEXIT_CRITICAL();

    for (;;)
    {
        next_tick += period_ticks;
        if (osDelayUntil(next_tick) != osOK)
        {
            /* 记录错过的调度点，并重新对齐，避免连续追赶造成忙循环。 */
            sample.delay_error_count++;
            next_tick = osKernelGetTickCount();
        }

        const uint32_t current_tick = osKernelGetTickCount();
        const uint32_t interval_ticks = current_tick - previous_tick;
        previous_tick = current_tick;

        sample.hal_tick_ms = HAL_GetTick();
        sample.rtos_tick = current_tick;
        sample.interval_ticks = interval_ticks;
        if (interval_ticks < sample.min_interval_ticks)
        {
            sample.min_interval_ticks = interval_ticks;
        }
        if (interval_ticks > sample.max_interval_ticks)
        {
            sample.max_interval_ticks = interval_ticks;
        }
        sample.stack_free_bytes = osThreadGetStackSpace(task);
        sample.sample_count++;
        taskENTER_CRITICAL();
        runtime_status = sample;
        taskEXIT_CRITICAL();
    }
}
