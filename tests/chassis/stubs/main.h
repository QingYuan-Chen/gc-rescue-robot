#ifndef TEST_MAIN_H
#define TEST_MAIN_H

#include <stdint.h>

typedef struct
{
    int unused;
} UART_HandleTypeDef;

uint32_t HAL_GetTick(void);

#endif
