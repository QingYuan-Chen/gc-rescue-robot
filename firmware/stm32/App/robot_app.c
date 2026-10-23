#include "robot_app.h"

#include "runtime/runtime_monitor.h"

#include "cmsis_os2.h"

#define ROBOT_HEARTBEAT_PERIOD_MS 10U

void robot_app_run(void)
{
    uint32_t next_tick = osKernelGetTickCount();
    runtime_monitor_init(ROBOT_HEARTBEAT_PERIOD_MS);
    for (;;)
    {
        next_tick += ROBOT_HEARTBEAT_PERIOD_MS;
        const bool delayed = osDelayUntil(next_tick) != osOK;
        const uint32_t tick = osKernelGetTickCount();
        if (delayed)
        {
            next_tick = tick;
        }
        /* 当前仅观察基础运行，不启动执行器或编码器。 */
        runtime_monitor_sample(tick, delayed);
    }
}
