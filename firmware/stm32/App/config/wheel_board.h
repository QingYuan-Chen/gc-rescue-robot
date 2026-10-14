#ifndef ROBOT_WHEEL_BOARD_H
#define ROBOT_WHEEL_BOARD_H

#include "main.h"
#include "tim.h"

/* PWM 频率、编码器滤波与引脚复用由 rescue.ioc 管理，不在此重复初始化。 */
#define WHEEL_PWM_TIMER htim12
/* 故障停机直接使用外设地址，不依赖 HAL 句柄的初始化状态。 */
#define WHEEL_PWM_INSTANCE TIM12
#define WHEEL_PWM_GPIO_PORT GPIOB
#define WHEEL_PWM_GPIO_PINS (PWMD_Pin | PWMC_Pin)
#define WHEEL_PWM_GPIO_MODE_MASK (GPIO_MODER_MODER14_Msk | GPIO_MODER_MODER15_Msk)
#define WHEEL_PWM_GPIO_OUTPUT_MODE (GPIO_MODER_MODER14_0 | GPIO_MODER_MODER15_0)

/* 现场拔线确认 D 接右轮，C 对应左轮；网表 CO/DO 电机网络名与插座名交叉。 */
#define WHEEL_LEFT_PWM_CHANNEL TIM_CHANNEL_2
#define WHEEL_RIGHT_PWM_CHANNEL TIM_CHANNEL_1
#define WHEEL_LEFT_IN1_PORT CIN1_GPIO_Port
#define WHEEL_LEFT_IN1_PIN CIN1_Pin
#define WHEEL_LEFT_IN2_PORT CIN2_GPIO_Port
#define WHEEL_LEFT_IN2_PIN CIN2_Pin
#define WHEEL_RIGHT_IN1_PORT DIN1_GPIO_Port
#define WHEEL_RIGHT_IN1_PIN DIN1_Pin
#define WHEEL_RIGHT_IN2_PORT DIN2_GPIO_Port
#define WHEEL_RIGHT_IN2_PIN DIN2_Pin
#define WHEEL_LEFT_ENCODER_TIMER htim4
#define WHEEL_RIGHT_ENCODER_TIMER htim8

/* 现场旧正输出使两轮后退，故两路电机均取反；正命令改为 IN1 低、IN2 高。 */
#define WHEEL_LEFT_MOTOR_SIGN -1
#define WHEEL_RIGHT_MOTOR_SIGN -1
/* 编码器随插座交换左右归属，计数方向暂不取反，待逐轮前进时实测确认。 */
#define WHEEL_LEFT_ENCODER_SIGN 1
#define WHEEL_RIGHT_ENCODER_SIGN 1

#endif
