#ifndef ROBOT_PID_COMMANDS_H
#define ROBOT_PID_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>

/* 仅由调试任务访问；草稿和等待回执的请求均不暴露给控制器。 */
void pid_commands_init(void);
bool pid_commands_execute(const char *line, char *response, size_t capacity);
/* 轮询控制任务的最终处理结果；入队回复与实际采用回复分别发送。 */
bool pid_commands_poll(char *response, size_t capacity);

#endif
