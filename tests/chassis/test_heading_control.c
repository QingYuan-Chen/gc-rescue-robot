#include "control/heading_control.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#define DEG_TO_RAD 0.017453292519943295f

static heading_control_config_t config(void)
{
    return (heading_control_config_t){.kp = 3.0f,
                                      .kd = 0.10f,
                                      .ki = 0.0f,
                                      .filter_ms = 20.0f,
                                      .wmax_dps = 90.0f,
                                      .accel_dps2 = 180.0f,
                                      .tol_deg = 2.0f,
                                      .rate_tol_dps = 5.0f,
                                      .izone_deg = 5.0f,
                                      .ilimit_dps = 5.0f,
                                      .settle_ms = 100U};
}

static heading_control_config_t integral_config(void)
{
    /* 小 Kp、大限幅，便于单独观察积分行为。 */
    return (heading_control_config_t){.kp = 1.0f,
                                      .kd = 0.0f,
                                      .ki = 1.0f,
                                      .filter_ms = 0.0f,
                                      .wmax_dps = 100.0f,
                                      .accel_dps2 = 1000.0f,
                                      .tol_deg = 0.1f,
                                      .rate_tol_dps = 0.1f,
                                      .izone_deg = 45.0f,
                                      .ilimit_dps = 5.0f,
                                      .settle_ms = 100U};
}

static void test_basic(void)
{
    const heading_control_config_t cfg = config();
    assert(heading_control_config_valid(&cfg));

    heading_control_t control;
    heading_control_init(&control);
    uint32_t now = 0U;
    heading_control_start(&control, 1.5707963f, now);
    now += 20U;
    float omega = heading_control_step(&control, &cfg, 0.0f, 0.0f, true, now);
    assert(omega > 0.0f && omega <= 3.6f + 0.01f);
    /* ki=0 时不产生积分，输出完全由 P、D 决定。 */
    assert(control.integral_dps == 0.0f);

    /* 航向到达目标且角速度收敛后，持续满足判据才置 settled。 */
    now += 20U;
    omega = heading_control_step(&control, &cfg, 1.5707963f, 0.0f, true, now);
    for (unsigned i = 0U; i < 10U; i++)
    {
        now += 20U;
        omega = heading_control_step(&control, &cfg, 1.5707963f, 0.0f, true, now);
    }
    assert(fabsf(omega) < 0.01f);
    assert(control.settled);

    heading_control_start(&control, -3.1415926f, now);
    now += 20U;
    omega = heading_control_step(&control, &cfg, 0.0f, 0.0f, true, now);
    assert(omega < 0.0f);
}

static void test_integral_separation(void)
{
    const heading_control_config_t cfg = integral_config();
    assert(heading_control_config_valid(&cfg));

    heading_control_t control;
    heading_control_init(&control);
    uint32_t now = 0U;
    heading_control_start(&control, 0.0f, now);
    /* 误差 10° 超过 izone 5°，只冻结积分、不清零。 */
    heading_control_config_t narrow = cfg;
    narrow.izone_deg = 5.0f;
    for (unsigned i = 0U; i < 10U; i++)
    {
        now += 20U;
        heading_control_step(&control, &narrow, -10.0f * DEG_TO_RAD, 0.0f, true, now);
    }
    assert(control.integral_dps == 0.0f);

    /* 误差回到积分区间后开始累加。 */
    now += 20U;
    heading_control_step(&control, &narrow, -4.0f * DEG_TO_RAD, 0.0f, true, now);
    assert(control.integral_dps > 0.0f);
}

static void test_integral_limit(void)
{
    const heading_control_config_t cfg = integral_config();
    heading_control_t control;
    heading_control_init(&control);
    uint32_t now = 0U;
    heading_control_start(&control, 0.0f, now);
    for (unsigned i = 0U; i < 400U; i++)
    {
        now += 20U;
        heading_control_step(&control, &cfg, -1.0f * DEG_TO_RAD, 0.0f, true, now);
    }
    assert(fabsf(control.integral_dps - cfg.ilimit_dps) < 1e-4f);

    /* 反向误差把积分拉回来，不会跳变。 */
    now += 20U;
    heading_control_step(&control, &cfg, 1.0f * DEG_TO_RAD, 0.0f, true, now);
    assert(control.integral_dps < cfg.ilimit_dps);
}

static void test_integral_saturation_block(void)
{
    heading_control_config_t cfg = integral_config();
    cfg.kp = 20.0f;
    cfg.izone_deg = 10.0f;
    cfg.wmax_dps = 90.0f;
    assert(heading_control_config_valid(&cfg));

    heading_control_t control;
    heading_control_init(&control);
    uint32_t now = 0U;
    heading_control_start(&control, 0.0f, now);
    /* 误差 5° 未超出 izone，但 P 项已到 wmax，不允许同向继续积分。 */
    for (unsigned i = 0U; i < 20U; i++)
    {
        now += 20U;
        heading_control_step(&control, &cfg, -5.0f * DEG_TO_RAD, 0.0f, true, now);
    }
    assert(control.integral_dps == 0.0f);
}

static void test_integral_freeze_and_reset(void)
{
    const heading_control_config_t cfg = integral_config();
    heading_control_t control;
    heading_control_init(&control);
    uint32_t now = 0U;
    heading_control_start(&control, 0.0f, now);
    for (unsigned i = 0U; i < 5U; i++)
    {
        now += 20U;
        heading_control_step(&control, &cfg, -1.0f * DEG_TO_RAD, 0.0f, true, now);
    }
    const float held = control.integral_dps;
    assert(held > 0.0f);

    /* allow_integral=false：积分冻结，但已积累的积分仍参与输出。 */
    now += 20U;
    const float omega = heading_control_step(&control, &cfg, -1.0f * DEG_TO_RAD, 0.0f, false,
                                             now);
    assert(control.integral_dps == held);
    assert(omega > 1.0f);

    /* 新动作开始清零积分。 */
    heading_control_start(&control, 0.0f, now);
    assert(control.integral_dps == 0.0f);
}

static void test_integral_ki_change_continuous(void)
{
    const heading_control_config_t cfg = integral_config();
    heading_control_t control;
    heading_control_init(&control);
    uint32_t now = 0U;
    heading_control_start(&control, 0.0f, now);
    for (unsigned i = 0U; i < 5U; i++)
    {
        now += 20U;
        heading_control_step(&control, &cfg, -1.0f * DEG_TO_RAD, 0.0f, true, now);
    }
    const float before = control.last_omega_dps;
    assert(fabsf(control.integral_dps) > 0.0f);

    /* 在线把 Ki 改为 0：积分量保留，输出不应跳变。 */
    heading_control_config_t frozen = cfg;
    frozen.ki = 0.0f;
    now += 20U;
    const float after = heading_control_step(&control, &frozen, -1.0f * DEG_TO_RAD, 0.0f,
                                             true, now);
    assert(fabsf(after - before) < 1e-5f);
}

static void test_config_range(void)
{
    heading_control_config_t bad = config();
    bad.tol_deg = 0.0f;
    assert(!heading_control_config_valid(&bad));
    bad = config();
    bad.settle_ms = 0U;
    assert(!heading_control_config_valid(&bad));
    bad = config();
    bad.ki = 2.5f;
    assert(!heading_control_config_valid(&bad));
    bad = config();
    bad.izone_deg = 0.0f;
    assert(!heading_control_config_valid(&bad));
    bad = config();
    bad.ilimit_dps = 60.0f;
    assert(!heading_control_config_valid(&bad));
}

int main(void)
{
    test_basic();
    test_integral_separation();
    test_integral_limit();
    test_integral_saturation_block();
    test_integral_freeze_and_reset();
    test_integral_ki_change_continuous();
    test_config_range();
    puts("PASS: heading control limits, damping, settle and integral");
    return 0;
}
