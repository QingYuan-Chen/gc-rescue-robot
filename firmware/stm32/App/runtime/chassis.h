#ifndef ROBOT_CHASSIS_H
#define ROBOT_CHASSIS_H

#include "control/heading_control.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CHASSIS_IDLE = 0,
    CHASSIS_VW,
    CHASSIS_TURN,
    CHASSIS_STRAIGHT
} chassis_mode_t;

typedef heading_control_config_t chassis_turn_config_t;
typedef heading_control_config_t chassis_hold_config_t;

typedef struct
{
    chassis_mode_t mode;
    float v_mm_s;
    float omega_deg_s;
    float left_rpm;
    float right_rpm;
    float target_yaw_deg;
    float yaw_deg;
    float yaw_error_deg;
    float yaw_rate_dps;
    float i_dps;
    float distance_mm;
    float distance_target_mm;
    uint32_t remaining_ms;
    bool saturated;
    bool imu_valid;
    bool settled;
    uint8_t stop_reason;
} chassis_status_t;

void chassis_init(void);
bool chassis_start_vw(float v_mm_s, float omega_deg_s, uint32_t duration_ms);
bool chassis_start_turn(float relative_deg);
bool chassis_start_straight(float v_mm_s, uint32_t duration_ms, float distance_mm);
void chassis_stop(void);
void chassis_step(uint32_t now_ms);
void chassis_get_status(chassis_status_t *status);

bool chassis_set_turn_config(const chassis_turn_config_t *config);
bool chassis_set_hold_config(const chassis_hold_config_t *config);
void chassis_get_turn_config(chassis_turn_config_t *config);
void chassis_get_hold_config(chassis_hold_config_t *config);

#endif
