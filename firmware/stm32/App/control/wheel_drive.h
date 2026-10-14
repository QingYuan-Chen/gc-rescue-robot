#ifndef ROBOT_WHEEL_DRIVE_H
#define ROBOT_WHEEL_DRIVE_H

#include "bsp/wheel_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t sample_tick_ms;
    uint32_t remaining_ms;
    int32_t duty_permille[WHEEL_COUNT];
    int32_t delta_counts[WHEEL_COUNT];
    int64_t total_counts[WHEEL_COUNT];
    uint32_t raw_counts[WHEEL_COUNT];
} wheel_drive_status_t;

bool wheel_drive_init(void);
void wheel_drive_step(uint32_t tick_ms);

/* 按左、右顺序提交双轮命令，在下一个周期执行；最后一条待处理命令生效。
 * 手动入口限幅、限时，到期自动停车，避免无线链路中断后持续转动。
 */
bool wheel_drive_request(int32_t duty_left, int32_t duty_right, uint32_t duration_ms);
void wheel_drive_stop(void);
void wheel_drive_get_status(wheel_drive_status_t *status);

#endif
