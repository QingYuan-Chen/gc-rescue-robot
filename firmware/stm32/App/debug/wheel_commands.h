#ifndef ROBOT_WHEEL_COMMANDS_H
#define ROBOT_WHEEL_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>

/* 只解析 wheel 命令；由控制台统一发送响应，不在此直接操作串口或硬件。 */
bool wheel_commands_execute(const char *line, char *response, size_t capacity);
void wheel_commands_format_status(char *response, size_t capacity);

#endif
