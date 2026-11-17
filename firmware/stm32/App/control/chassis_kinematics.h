#ifndef ROBOT_CHASSIS_KINEMATICS_H
#define ROBOT_CHASSIS_KINEMATICS_H

#include <stdbool.h>

typedef struct
{
    float left_rpm;
    float right_rpm;
    bool saturated;
} chassis_wheel_targets_t;

/* v 单位 mm/s，omega 单位 rad/s；转向优先，超出轮速范围时先限制角速度再限制线速度。 */
bool chassis_kinematics(float v_mm_s, float omega_rad_s, chassis_wheel_targets_t *targets);

#endif
