#ifndef ROBOT_DEBUG_UART_CONFIG_H
#define ROBOT_DEBUG_UART_CONFIG_H

#include "usart.h"

/* PB12 接收、PB13 发送；115200、8N1 的外设配置由 rescue.ioc 生成。 */
#define DEBUG_UART_HANDLE huart5
#define DEBUG_UART_RX_QUEUE_LENGTH 256U
#define DEBUG_UART_TX_TIMEOUT_MS 100U

#endif
