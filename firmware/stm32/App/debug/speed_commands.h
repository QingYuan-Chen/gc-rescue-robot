#ifndef ROBOT_SPEED_COMMANDS_H
#define ROBOT_SPEED_COMMANDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool speed_commands_execute(char *line, char *response, size_t capacity);
bool speed_commands_wave_due(uint32_t tick, char *response, size_t capacity);
void speed_commands_wave_dropped(void);

#endif
