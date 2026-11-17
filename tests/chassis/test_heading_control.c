#include "control/heading_control.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static heading_control_config_t config(void)
{
    return (heading_control_config_t){.kp = 3.0f,
                                      .kd = 0.10f,
                                      .filter_ms = 20.0f,
                                      .wmax_dps = 90.0f,
                                      .accel_dps2 = 180.0f,
                                      .tol_deg = 2.0f,
                                      .rate_tol_dps = 5.0f,
                                      .settle_ms = 100U};
}

int main(void)
{
    const heading_control_config_t cfg = config();
    assert(heading_control_config_valid(&cfg));

    heading_control_t control;
    heading_control_init(&control);
    uint32_t now = 0U;
    heading_control_start(&control, 1.5707963f, now);
    now += 20U;
    float omega = heading_control_step(&control, &cfg, 0.0f, 0.0f, now);
    assert(omega > 0.0f && omega <= 3.6f + 0.01f);

    /* 航向到达目标且角速度收敛后，持续满足判据才置 settled。 */
    now += 20U;
    omega = heading_control_step(&control, &cfg, 1.5707963f, 0.0f, now);
    for (unsigned i = 0U; i < 10U; i++)
    {
        now += 20U;
        omega = heading_control_step(&control, &cfg, 1.5707963f, 0.0f, now);
    }
    assert(fabsf(omega) < 0.01f);
    assert(control.settled);

    heading_control_start(&control, -3.1415926f, now);
    now += 20U;
    omega = heading_control_step(&control, &cfg, 0.0f, 0.0f, now);
    assert(omega < 0.0f);

    heading_control_config_t bad = cfg;
    bad.tol_deg = 0.0f;
    assert(!heading_control_config_valid(&bad));
    bad = cfg;
    bad.settle_ms = 0U;
    assert(!heading_control_config_valid(&bad));
    puts("PASS: heading control limits, damping and settle");
    return 0;
}
