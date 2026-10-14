#ifndef ROBOT_WHEEL_PID_CONFIG_H
#define ROBOT_WHEEL_PID_CONFIG_H

/* 来源：ae7b14b 的 gc_rescue/Motor/motor.c，四组原速度 PID 参数相同。
 * 输入为每 10 ms 的编码器计数，输出保留原 TIM1 CCR 计数尺度。
 */
#define WHEEL_PID_REFERENCE_PERIOD_MS 10U
#define WHEEL_PID_KP 1600.0f
#define WHEEL_PID_KI 180.0f
#define WHEEL_PID_KD 0.0f

/* 原 TIM1 ARR=1999，完整 PWM 周期为 2000；限幅为截断 (1999 * 0.8f)=1599。
 * 此处保存原算法的单位基准，不读取现用于舵机的 TIM1，也不修改当前 TIM12。
 */
#define WHEEL_PID_REFERENCE_PWM_PERIOD_COUNTS 2000
#define WHEEL_PID_OUTPUT_LIMIT_COUNTS 1599

#endif
