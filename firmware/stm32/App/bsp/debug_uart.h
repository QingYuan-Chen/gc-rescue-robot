#ifndef ROBOT_DEBUG_UART_H
#define ROBOT_DEBUG_UART_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t rx_bytes;
    uint32_t tx_bytes;
    uint32_t rx_overflow_count;
    uint32_t rx_error_count;
    uint32_t tx_error_count;
} debug_uart_status_t;

/* 外设初始化、调度器启动后，由唯一的调试任务调用一次。 */
bool debug_uart_start(void);

/* 只允许调试任务读写；data_lost 表示当前字节之前发生过丢字节。 */
bool debug_uart_read(uint8_t *byte, bool *data_lost, uint32_t timeout_ms);

/* 中断发送，等待期间让出 CPU；返回前保证不再引用调用方的缓冲区。 */
bool debug_uart_write(const char *data, uint16_t length);
void debug_uart_get_status(debug_uart_status_t *status);

#endif
