#include "runtime_monitor.h"

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "stm32h7xx_hal.h"
#include "task.h"

/* 统计由控制任务独占更新；公开快照通过临界区交接，保留 SWD 观察能力。 */
static runtime_monitor_status_t sample;
static volatile runtime_monitor_status_t runtime_status;
static uint32_t previous_tick;
static osThreadId_t control_task;

void runtime_monitor_get_status(runtime_monitor_status_t *status)
{
    taskENTER_CRITICAL();
    *status = runtime_status;
    taskEXIT_CRITICAL();
}

void runtime_monitor_init(uint32_t period_ms)
{
    control_task = osThreadGetId();
    previous_tick = osKernelGetTickCount();
    sample = (runtime_monitor_status_t){
        .system_clock_hz = SystemCoreClock,
        .rtos_tick_hz = osKernelGetTickFreq(),
        .period_ticks = period_ms,
        .min_interval_ticks = UINT32_MAX,
    };
    taskENTER_CRITICAL();
    runtime_status = sample;
    taskEXIT_CRITICAL();
}

void runtime_monitor_sample(uint32_t tick, bool delay_error)
{
    sample.interval_ticks = tick - previous_tick;
    previous_tick = tick;
    sample.hal_tick_ms = HAL_GetTick();
    sample.rtos_tick = tick;
    if (sample.interval_ticks < sample.min_interval_ticks)
    {
        sample.min_interval_ticks = sample.interval_ticks;
    }
    if (sample.interval_ticks > sample.max_interval_ticks)
    {
        sample.max_interval_ticks = sample.interval_ticks;
    }
    sample.delay_error_count += delay_error ? 1U : 0U;
    sample.stack_free_bytes = osThreadGetStackSpace(control_task);
    sample.sample_count++;
    taskENTER_CRITICAL();
    runtime_status = sample;
    taskEXIT_CRITICAL();
}
