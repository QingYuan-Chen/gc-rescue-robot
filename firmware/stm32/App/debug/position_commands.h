#ifndef ROBOT_POSITION_COMMANDS_H
#define ROBOT_POSITION_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>

bool position_commands_execute(char *line, char *response, size_t capacity);

#endif
