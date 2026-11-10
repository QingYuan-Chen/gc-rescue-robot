#ifndef ROBOT_MOTOR_COMMANDS_H
#define ROBOT_MOTOR_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>

/* 返回是否识别到本模块命令；只投递请求或读取快照，不在调试任务写执行器。 */
bool motor_commands_execute(const char *line, char *response, size_t capacity);
void motor_commands_status(char *response, size_t capacity);

#endif
