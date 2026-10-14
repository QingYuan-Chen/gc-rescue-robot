#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H

/* 主机不运行调度器，仅为正式模块提供与固件一致的 tick 配置。 */
#define configTICK_RATE_HZ 1000U

#endif
