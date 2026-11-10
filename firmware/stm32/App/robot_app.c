#include "robot_app.h"

#include "runtime/runtime_monitor.h"
#include "runtime/wheel_io.h"
#include "config/wheel_config.h"
#include "main.h"

#include "cmsis_os2.h"

void robot_app_run(void)
{
    /* FreeRTOS 启动入口已校验 1 kHz tick，此处 ms 与 tick 一一对应。 */
    uint32_t next_tick = osKernelGetTickCount();
    runtime_monitor_init(WHEEL_SAMPLE_PERIOD_MS);
    if (!wheel_io_init(next_tick))
    {
        Error_Handler();
    }
    for (;;)
    {
        /* 按绝对时刻推进，避免把每轮处理耗时叠加进采样周期。 */
        next_tick += WHEEL_SAMPLE_PERIOD_MS;
        const bool delayed = osDelayUntil(next_tick) != osOK;
        const uint32_t tick = osKernelGetTickCount();
        if (delayed)
        {
            next_tick = tick;
        }
        /* 所有正常电机输出均由该任务执行；无有效命令时保持双低。 */
        wheel_io_step(tick, delayed);
        runtime_monitor_sample(tick, delayed);
    }
}
