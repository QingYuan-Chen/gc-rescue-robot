#include "control/chassis_kinematics.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void expect(float v, float omega, float left, float right, bool saturated)
{
    chassis_wheel_targets_t targets;
    chassis_kinematics(v, omega, &targets);
    assert(fabsf(targets.left_rpm - left) < 0.05f);
    assert(fabsf(targets.right_rpm - right) < 0.05f);
    assert(targets.saturated == saturated);
}

int main(void)
{
    expect(0.0f, 0.0f, 0.0f, 0.0f, false);
    expect(200.0f, 0.0f, 58.77f, 58.77f, false);
    expect(0.0f, 1.0f, -32.32f, 32.32f, false);
    expect(200.0f, 1.0f, 26.45f, 91.09f, false);
    expect(0.0f, -1.0f, 32.32f, -32.32f, false);
    expect(0.0f, 10.0f, -81.0f, 81.0f, true);
    expect(1000.0f, 0.0f, 97.0f, 97.0f, true);

    puts("PASS: chassis kinematics and saturation");
    return 0;
}
