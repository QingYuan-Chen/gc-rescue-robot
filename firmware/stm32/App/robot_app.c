#include "robot_app.h"

#include "bsp/board_startup.h"
#include "runtime/runtime_monitor.h"

void robot_app_init(void)
{
    board_startup_hold_outputs();
}

void robot_app_run(void)
{
    runtime_monitor_run();
}
