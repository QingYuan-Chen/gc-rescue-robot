#ifndef ROBOT_ENCODER_H
#define ROBOT_ENCODER_H

#include "wheel_types.h"

#include <stdbool.h>
#include <stdint.h>

bool encoder_init(void);

/* 仅车轮周期任务每 10 ms 调用；保留自由运行计数器，避免读后清零丢失边沿。
 * 单周期位移须小于计数范围的一半，返回配置方向后的有符号增量。
 */
int32_t encoder_read_delta_counts(wheel_id_t wheel, uint32_t *raw_count);

#endif
