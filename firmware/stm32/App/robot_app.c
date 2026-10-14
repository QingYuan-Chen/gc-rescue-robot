#include "robot_app.h"

#include "bsp/board_startup.h"
#include "config/wheel_config.h"
#include "control/wheel_drive.h"
#include "runtime/runtime_monitor.h"

#include "cmsis_os2.h"
#include "main.h"

void robot_app_init(void)
{
    board_startup_hold_outputs();
}

void robot_app_run(void)
{
    if (!wheel_drive_init())
    {
        Error_Handler();
    }
    uint32_t next_tick = osKernelGetTickCount();
    wheel_drive_step(next_tick, false);
    runtime_monitor_init(WHEEL_PERIOD_MS);
    for (;;)
    {
        next_tick += WHEEL_PERIOD_MS;
        const bool delayed = osDelayUntil(next_tick) != osOK;
        const uint32_t tick = osKernelGetTickCount();
        if (delayed)
        {
            /* 控制层负责锁定停车；调度重新对齐后仍继续采样和报告。 */
            next_tick = tick;
        }
        wheel_drive_step(tick, delayed);
        runtime_monitor_sample(tick, delayed);
    }
}
