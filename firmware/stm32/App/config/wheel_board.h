#ifndef ROBOT_WHEEL_BOARD_H
#define ROBOT_WHEEL_BOARD_H

#include "main.h"
#include "tim.h"

/* PWM 频率、编码器滤波与引脚复用由 rescue.ioc 管理，不在此重复初始化。 */
#define WHEEL_PWM_TIMER htim15
/* 故障停机直接使用外设地址，不依赖 HAL 句柄的初始化状态。 */
#define WHEEL_PWM_INSTANCE TIM15
#define WHEEL_A_PWM_CHANNEL TIM_CHANNEL_1
#define WHEEL_B_PWM_CHANNEL TIM_CHANNEL_2
#define WHEEL_A_IN1_PORT AIN1_GPIO_Port
#define WHEEL_A_IN1_PIN AIN1_Pin
#define WHEEL_A_IN2_PORT AIN2_GPIO_Port
#define WHEEL_A_IN2_PIN AIN2_Pin
#define WHEEL_B_IN1_PORT BIN1_GPIO_Port
#define WHEEL_B_IN1_PIN BIN1_Pin
#define WHEEL_B_IN2_PORT BIN2_GPIO_Port
#define WHEEL_B_IN2_PIN BIN2_Pin
#define WHEEL_A_ENCODER_TIMER htim3
#define WHEEL_B_ENCODER_TIMER htim2

/* 暂沿用原 A/B 正方向：IN1 高、IN2 低，编码器计数不取反；待逐轮实测确认。 */
#define WHEEL_A_MOTOR_SIGN 1
#define WHEEL_B_MOTOR_SIGN 1
#define WHEEL_A_ENCODER_SIGN 1
#define WHEEL_B_ENCODER_SIGN 1

#endif
