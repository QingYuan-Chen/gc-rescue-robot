#ifndef Maixcam_H
#define Maixcam_H

#include "common.h"
#include "string.h"

#define danger_mode 0x01
#define normal_mode 0x02
#define base_mode 0x03

void Maixcam_init(void);
void Set_Vision_Mode(uint8_t mode);

#endif
