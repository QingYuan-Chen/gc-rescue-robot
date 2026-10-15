#include "control/pid.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned checks;

#define CHECK(condition)                                                                           \
    do                                                                                             \
    {                                                                                              \
        checks++;                                                                                  \
        if (!(condition))                                                                          \
        {                                                                                          \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition);                           \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static void pure_p_and_reset(void)
{
    const pid_pi_params_t params = {.kp = 100, .ki = 0, .tracking_gain_per_s = 10};
    pid_pi_state_t state = {0};
    CHECK(pid_pi_prepare(&state, &params, 10, 0) == 1000);
    pid_pi_track(&state, &params, 0.01f, 250, true);
    CHECK(state.next_integral == 0);
    CHECK(pid_pi_prepare(&state, &params, 0, 0) == 0);
    CHECK(pid_pi_prepare(&state, &params, -1, 0) == -100);
    pid_pi_track(&state, &params, 0.01f, 0, true);
    CHECK(state.next_integral == 0);
    CHECK(pid_pi_prepare(&state, &params, -1, 0) == -100);
    pid_pi_reset(&state);
    CHECK(state.output == 0 && state.proportional == 0 && state.error == 0);
    CHECK(state.integral == 0 && state.next_integral == 0);
}

static void explicit_time_and_zero_target(void)
{
    const pid_pi_params_t params = {.kp = 2, .ki = 5, .tracking_gain_per_s = 10};
    pid_pi_state_t slow = {0}, fast = {0};
    for (unsigned i = 0; i < 10; i++)
    {
        const float requested = pid_pi_prepare(&slow, &params, 2, 0);
        CHECK(fabsf(requested - (4.0f + 0.1f * (float)i)) < 0.0001f);
        pid_pi_track(&slow, &params, 0.01f, requested, false);
    }
    for (unsigned i = 0; i < 20; i++)
    {
        const float requested = pid_pi_prepare(&fast, &params, 2, 0);
        pid_pi_track(&fast, &params, 0.005f, requested, false);
    }
    CHECK(fabsf(slow.next_integral - 1) < 0.0001f);
    CHECK(fabsf(slow.next_integral - fast.next_integral) < 0.0001f);
    CHECK(fabsf(pid_pi_prepare(&slow, &params, 0, 0) - 1) < 0.0001f);
    pid_pi_track(&slow, &params, 0.01f, slow.output, false);
    CHECK(fabsf(slow.next_integral - 1) < 0.0001f);
}

static void tracking_reversal_and_quantization(void)
{
    const pid_pi_params_t params = {.kp = 80, .ki = 400, .tracking_gain_per_s = 10};
    pid_pi_state_t state = {0};
    CHECK(pid_pi_prepare(&state, &params, 1, 0) == 80);
    pid_pi_track(&state, &params, 0.01f, 80, false);
    CHECK(state.next_integral == 4);
    CHECK(pid_pi_prepare(&state, &params, -1, 0) == -76);
    pid_pi_track(&state, &params, 0.01f, 0, true);
    CHECK(state.integral == 4 && state.output == -76);
    CHECK(fabsf(state.next_integral - 7.6f) < 0.0001f);

    /* 0.1 千分比/周期的积分要能越过一个 PWM 计数，不能被取整回算抵消。 */
    const pid_pi_params_t small = {.kp = 0, .ki = 10, .tracking_gain_per_s = 10};
    pid_pi_reset(&state);
    for (unsigned i = 0; i < 15; i++)
    {
        const float requested = pid_pi_prepare(&state, &small, 1, 0);
        pid_pi_track(&state, &small, 0.01f, (float)(int)requested, false);
    }
    CHECK(state.output > 1 && state.output < 2);
    CHECK(fabsf(state.next_integral - 1.5f) < 0.0001f);
}

static void positive_and_negative_saturation(void)
{
    const pid_pi_params_t params = {.kp = 80, .ki = 400, .tracking_gain_per_s = 10};
    for (int sign = -1; sign <= 1; sign += 2)
    {
        pid_pi_state_t state = {0};
        for (unsigned i = 0; i < 1000; i++)
        {
            const float requested = pid_pi_prepare(&state, &params, (float)(10 * sign), 0);
            CHECK(requested * (float)sign > 250);
            pid_pi_track(&state, &params, 0.01f, (float)(250 * sign), true);
            CHECK(fabsf(state.next_integral) <= 150.01f);
        }
        CHECK(fabsf(state.next_integral + (float)(150 * sign)) < 0.01f);
        const float recovered = pid_pi_prepare(&state, &params, (float)(3 * sign), 0);
        CHECK(fabsf(recovered - (float)(90 * sign)) < 0.01f);
    }
}

int main(void)
{
    pure_p_and_reset();
    explicit_time_and_zero_target();
    tracking_reversal_and_quantization();
    positive_and_negative_saturation();
    printf("PASS: 4 PI scenarios, %u checks\n", checks);
    return 0;
}
