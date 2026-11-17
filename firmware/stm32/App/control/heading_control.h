#ifndef ROBOT_HEADING_CONTROL_H
#define ROBOT_HEADING_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    float kp;
    float kd;
    float filter_ms;
    float wmax_dps;
    float accel_dps2;
    float tol_deg;
    float rate_tol_dps;
    uint32_t settle_ms;
} heading_control_config_t;

typedef struct
{
    float target_rad;
    float last_omega_dps;
    float filtered_rate_dps;
    uint32_t last_ms;
    uint32_t settle_start_ms;
    bool started;
    bool settled;
} heading_control_t;

void heading_control_init(heading_control_t *control);
bool heading_control_config_valid(const heading_control_config_t *config);
void heading_control_start(heading_control_t *control, float target_rad, uint32_t now_ms);
/* 返回本次角速度命令；settled 只表示持续满足结束判据，不停车。 */
float heading_control_step(heading_control_t *control, const heading_control_config_t *config,
                           float yaw_rad, float yaw_rate_rad_s, uint32_t now_ms);

#endif
