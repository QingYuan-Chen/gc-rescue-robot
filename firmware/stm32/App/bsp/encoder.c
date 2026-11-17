#include "encoder.h"

#include "tim.h"

static encoder_sample_t current;

bool encoder_init(void)
{
    /* 仅启动时清零；运行中差分采样不清 CNT，避免漏计采样之间的边沿。 */
    current = (encoder_sample_t){0};
    __HAL_TIM_SET_COUNTER(&htim3, 0U);
    __HAL_TIM_SET_COUNTER(&htim5, 0U);
    if (HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL) != HAL_OK)
    {
        return false;
    }
    if (HAL_TIM_Encoder_Start(&htim5, TIM_CHANNEL_ALL) != HAL_OK)
    {
        /* 两轮初始化视为整体：第二路失败时撤销已启动的第一路计数。 */
        HAL_TIM_Encoder_Stop(&htim3, TIM_CHANNEL_ALL);
        return false;
    }
    return true;
}

static int32_t counter_delta(uint16_t value, uint16_t previous)
{
    const uint16_t difference = (uint16_t)(value - previous);
    /* 两次采样之间位移必须小于半个计数范围，跨 0/65535 时仍保留正确符号。 */
    return difference < 32768U ? (int32_t)difference : (int32_t)difference - 65536;
}

void encoder_sample(encoder_sample_t *sample)
{
    /* 左 TIM5、右 TIM3，各自差分；顺序读取不是硬件同时锁存。 */
    const uint16_t left = (uint16_t)__HAL_TIM_GET_COUNTER(&htim5);
    const uint16_t right = (uint16_t)__HAL_TIM_GET_COUNTER(&htim3);
    current.delta_left = -counter_delta(left, current.counter_left);
    current.delta_right = counter_delta(right, current.counter_right);
    current.counter_left = left;
    current.counter_right = right;
    /* 左轮按实测方向取反；累计使用 64 位，单位为 count。 */
    current.total_left += current.delta_left;
    current.total_right += current.delta_right;
    *sample = current;
}
