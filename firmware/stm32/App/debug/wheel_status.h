#ifndef ROBOT_WHEEL_STATUS_H
#define ROBOT_WHEEL_STATUS_H

#include <stddef.h>

/* 将同一周期的车轮快照格式化，由控制台发送；不读写硬件。 */
void wheel_status_format(char *response, size_t capacity);

#endif
