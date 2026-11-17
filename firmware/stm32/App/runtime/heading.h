#ifndef ROBOT_HEADING_H
#define ROBOT_HEADING_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float yaw_rad;
    float yaw_rate_rad_s;
    uint32_t age_ms;
    uint32_t angle_frames;
    uint32_t gyro_frames;
    uint32_t frame_period_ms;
    bool valid;
    bool jump_error;
} heading_status_t;

void heading_init(void);
/* now_ms 必须与 imu 时间戳使用同一时基，调用方传 HAL_GetTick()。 */
void heading_update(uint32_t now_ms);
void heading_get_status(heading_status_t *status);
bool heading_valid(void);

#endif
