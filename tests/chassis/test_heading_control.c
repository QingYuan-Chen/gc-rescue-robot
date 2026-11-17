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
                                      .brake_dps2 = 0.0f,
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
                                      .brake_dps2 = 0.0f,
                                      .settle_ms = 100U};
}

static heading_control_config_t brake_config(void)
{
    /* 转向默认参数：Kp 3、brake 100、Kd 0。 */
    return (heading_control_config_t){.kp = 3.0f,
                                      .kd = 0.0f,
                                      .ki = 0.0f,
                                      .filter_ms = 20.0f,
                                      .wmax_dps = 90.0f,
                                      .accel_dps2 = 180.0f,
                                      .tol_deg = 2.0f,
                                      .rate_tol_dps = 5.0f,
                                      .izone_deg = 5.0f,
                                      .ilimit_dps = 0.0f,
                                      .brake_dps2 = 100.0f,
                                      .settle_ms = 100U};
}

/* 固定航向偏差下，反复调用直到角速度命令稳定，返回稳态输出。 */
static float steady_output(const heading_control_config_t *cfg, float error_deg)
{
    heading_control_t control;
    heading_control_init(&control);
    uint32_t now = 0U;
    heading_control_start(&control, 0.0f, now);
    float omega = 0.0f;
    for (unsigned i = 0U; i < 20U; i++)
    {
        now += 20U;
        omega = heading_control_step(&control, cfg, -error_deg * DEG_TO_RAD, 0.0f, false,
                                     now);
    }
    return omega;
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
    bad = config();
    bad.brake_dps2 = -1.0f;
    assert(!heading_control_config_valid(&bad));
    bad = config();
    bad.brake_dps2 = config().accel_dps2 + 1.0f;
    assert(!heading_control_config_valid(&bad));
    bad = config();
    bad.brake_dps2 = bad.accel_dps2;
    assert(heading_control_config_valid(&bad));
}

static void test_brake_curve(void)
{
    heading_control_config_t cfg = brake_config();
    /* 放开限幅，直接观察位置项输出。 */
    cfg.wmax_dps = 200.0f;
    cfg.accel_dps2 = 1000.0f;
    assert(heading_control_config_valid(&cfg));

    /* 切换点 e1 = brake/Kp²；两段值与斜率连续。 */
    const float e1 = cfg.brake_dps2 / (cfg.kp * cfg.kp);
    const float below = steady_output(&cfg, e1 - 0.001f);
    const float at = steady_output(&cfg, e1);
    const float above = steady_output(&cfg, e1 + 0.001f);
    assert(fabsf(at - cfg.kp * e1) < 0.05f);
    assert(fabsf(above - at) < 0.05f);
    assert(above > at && at > below);

    /* 大偏差走制动段：比线性 P 更早减速，且符合 sqrt 曲线。 */
    const float far_brake = steady_output(&cfg, 60.0f);
    heading_control_config_t linear = cfg;
    linear.brake_dps2 = 0.0f;
    const float far_linear = steady_output(&linear, 60.0f);
    assert(far_brake < far_linear);
    assert(fabsf(far_brake - sqrtf(2.0f * cfg.brake_dps2 * (60.0f - 0.5f * e1))) < 0.05f);

    /* 负偏差对称。 */
    heading_control_t control;
    heading_control_init(&control);
    uint32_t now = 0U;
    heading_control_start(&control, 0.0f, now);
    float omega = 0.0f;
    for (unsigned i = 0U; i < 20U; i++)
    {
        now += 20U;
        omega = heading_control_step(&control, &cfg, 60.0f * DEG_TO_RAD, 0.0f, false, now);
    }
    assert(fabsf(omega + far_brake) < 0.05f);
}

typedef struct
{
    float overshoot_deg;
    uint32_t settle_ms;
    bool settled;
} motion_result_t;

/* 简化闭环：角速度命令经一阶滞后驱动航向，用于区分制动曲线与纯线性 P。 */
static motion_result_t simulate_turn(const heading_control_config_t *cfg, float target_deg)
{
    heading_control_t control;
    heading_control_init(&control);
    uint32_t now = 0U;
    heading_control_start(&control, target_deg * DEG_TO_RAD, now);
    float yaw_deg = 0.0f;
    float rate_dps = 0.0f;
    motion_result_t result = {0};

    for (unsigned i = 0U; i < 400U; i++)
    {
        now += 20U;
        const float omega = heading_control_step(&control, cfg, yaw_deg * DEG_TO_RAD,
                                                 rate_dps * DEG_TO_RAD, false, now);
        /* 速度环等效一阶滞后 150 ms，覆盖采样、滤波和电机响应。 */
        rate_dps += (omega - rate_dps) * (20.0f / 170.0f);
        yaw_deg += rate_dps * 0.02f;
        const float overshoot = yaw_deg - target_deg;
        if (overshoot > result.overshoot_deg) { result.overshoot_deg = overshoot; }
        if (control.settled)
        {
            result.settled = true;
            result.settle_ms = now;
            break;
        }
    }
    return result;
}

static void test_brake_motion(void)
{
    const heading_control_config_t cfg = brake_config();
    const float angles[] = {30.0f, 40.0f, 90.0f, 180.0f};
    for (unsigned i = 0U; i < sizeof(angles) / sizeof(angles[0]); i++)
    {
        const motion_result_t result = simulate_turn(&cfg, angles[i]);
        assert(result.settled);
        assert(result.overshoot_deg < 2.5f);
        assert(result.settle_ms <= 5000U);
    }

    /* 同一被控对象，纯线性 P（无 D、无制动曲线）在 180° 会明显超调。 */
    heading_control_config_t linear = cfg;
    linear.brake_dps2 = 0.0f;
    const motion_result_t linear_result = simulate_turn(&linear, 180.0f);
    const motion_result_t brake_result = simulate_turn(&cfg, 180.0f);
    assert(linear_result.overshoot_deg > 3.5f);
    assert(brake_result.overshoot_deg < 0.5f * linear_result.overshoot_deg);
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
    test_brake_curve();
    test_brake_motion();
    puts("PASS: heading control limits, damping, settle, integral and brake curve");
    return 0;
}
