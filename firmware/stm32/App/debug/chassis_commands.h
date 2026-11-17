#ifndef ROBOT_CHASSIS_COMMANDS_H
#define ROBOT_CHASSIS_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>

bool chassis_commands_execute(char *line, char *response, size_t capacity);

#endif
