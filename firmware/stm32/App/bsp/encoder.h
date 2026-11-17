#ifndef ROBOT_ENCODER_H
#define ROBOT_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    /* 原始 16 位 CNT，保留硬件计数方向。 */
    uint16_t counter_left;
    uint16_t counter_right;
    /* 相对上次采样的增量 count；双轮前进为正，间隔由 wheel_io 快照给出。 */
    int32_t delta_left;
    int32_t delta_right;
    /* 自初始化起的累计 count；仅复位/初始化清零。 */
    int64_t total_left;
    int64_t total_right;
} encoder_sample_t;

/* 仅由车轮任务初始化、采样；增量与累计按车体前进方向对齐。 */
bool encoder_init(void);
void encoder_sample(encoder_sample_t *sample);

#endif
