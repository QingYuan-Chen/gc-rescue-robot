#include "motor.h"

#include "config/wheel_board.h"

typedef struct
{
    uint32_t channel;
    GPIO_TypeDef *in1_port;
    uint16_t in1_pin;
    GPIO_TypeDef *in2_port;
    uint16_t in2_pin;
    int32_t sign;
} motor_channel_t;

static const motor_channel_t channels[WHEEL_COUNT] = {
    {WHEEL_A_PWM_CHANNEL, WHEEL_A_IN1_PORT, WHEEL_A_IN1_PIN, WHEEL_A_IN2_PORT, WHEEL_A_IN2_PIN,
     WHEEL_A_MOTOR_SIGN},
    {WHEEL_B_PWM_CHANNEL, WHEEL_B_IN1_PORT, WHEEL_B_IN1_PIN, WHEEL_B_IN2_PORT, WHEEL_B_IN2_PIN,
     WHEEL_B_MOTOR_SIGN},
};
static int32_t applied_duty[WHEEL_COUNT];

void motor_set_duty_permille(wheel_id_t wheel, int32_t duty_permille)
{
    if ((unsigned)wheel >= WHEEL_COUNT)
    {
        return;
    }
    if (duty_permille > 1000)
    {
        duty_permille = 1000;
    }
    else if (duty_permille < -1000)
    {
        duty_permille = -1000;
    }

    const motor_channel_t *channel = &channels[wheel];
    const int32_t directed_duty = duty_permille * channel->sign;
    if (directed_duty == 0 || applied_duty[wheel] == 0 ||
        (directed_duty > 0) != (applied_duty[wheel] > 0))
    {
        /* 禁用 CCR 预装载后，先写零立即停止脉冲，再改变方向脚。 */
        __HAL_TIM_SET_COMPARE(&WHEEL_PWM_TIMER, channel->channel, 0U);
        HAL_GPIO_WritePin(channel->in1_port, channel->in1_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(channel->in2_port, channel->in2_pin, GPIO_PIN_RESET);
        if (directed_duty > 0)
        {
            HAL_GPIO_WritePin(channel->in1_port, channel->in1_pin, GPIO_PIN_SET);
        }
        else if (directed_duty < 0)
        {
            HAL_GPIO_WritePin(channel->in2_port, channel->in2_pin, GPIO_PIN_SET);
        }
    }

    const uint32_t magnitude = (uint32_t)(directed_duty < 0 ? -directed_duty : directed_duty);
    const uint32_t period_counts = __HAL_TIM_GET_AUTORELOAD(&WHEEL_PWM_TIMER) + 1U;
    __HAL_TIM_SET_COMPARE(&WHEEL_PWM_TIMER, channel->channel, magnitude * period_counts / 1000U);
    applied_duty[wheel] = directed_duty;
}

bool motor_init(void)
{
    for (wheel_id_t wheel = WHEEL_A; wheel < WHEEL_COUNT; wheel++)
    {
        __HAL_TIM_DISABLE_OCxPRELOAD(&WHEEL_PWM_TIMER, channels[wheel].channel);
        motor_set_duty_permille(wheel, 0);
    }
    for (wheel_id_t wheel = WHEEL_A; wheel < WHEEL_COUNT; wheel++)
    {
        if (HAL_TIM_PWM_Start(&WHEEL_PWM_TIMER, channels[wheel].channel) != HAL_OK)
        {
            return false;
        }
    }
    return true;
}
