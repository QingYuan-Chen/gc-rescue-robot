#include "encoder.h"

#include "config/wheel_board.h"

static TIM_HandleTypeDef *const timers[WHEEL_COUNT] = {
    &WHEEL_A_ENCODER_TIMER,
    &WHEEL_B_ENCODER_TIMER,
};
static const int32_t signs[WHEEL_COUNT] = {WHEEL_A_ENCODER_SIGN, WHEEL_B_ENCODER_SIGN};
static uint32_t previous_counts[WHEEL_COUNT];

bool encoder_init(void)
{
    for (wheel_id_t wheel = WHEEL_A; wheel < WHEEL_COUNT; wheel++)
    {
        if (HAL_TIM_Encoder_Start(timers[wheel], TIM_CHANNEL_ALL) != HAL_OK)
        {
            return false;
        }
        previous_counts[wheel] = __HAL_TIM_GET_COUNTER(timers[wheel]);
    }
    return true;
}

int32_t encoder_read_delta_counts(wheel_id_t wheel, uint32_t *raw_count)
{
    if ((unsigned)wheel >= WHEEL_COUNT)
    {
        *raw_count = 0U;
        return 0;
    }
    const uint32_t current = __HAL_TIM_GET_COUNTER(timers[wheel]);
    const uint32_t mask = __HAL_TIM_GET_AUTORELOAD(timers[wheel]);
    const uint32_t difference = (current - previous_counts[wheel]) & mask;
    previous_counts[wheel] = current;
    *raw_count = current;

    /* TIM3 为 16 位、TIM2 为 32 位，均使用全量程；无符号差值处理跨零回绕。 */
    int64_t delta = difference;
    if (difference > mask / 2U)
    {
        delta -= (int64_t)mask + 1;
    }
    return (int32_t)(delta * signs[wheel]);
}
