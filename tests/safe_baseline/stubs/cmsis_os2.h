#ifndef TEST_CMSIS_OS2_H
#define TEST_CMSIS_OS2_H
#include <stdint.h>
typedef void *osThreadId_t;
uint32_t osKernelGetTickCount(void);
osThreadId_t osThreadGetId(void);
uint32_t osThreadGetStackSpace(osThreadId_t thread);
#endif
