#include "debug_uart.h"
#include "imu.h"

#include "config/debug_uart_config.h"

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "task.h"
#include <string.h>

#define DEBUG_UART_TX_DONE 1U

typedef struct
{
    /* 丢字节后代数递增，让任务识别队列中跨越丢失点的半条命令。 */
    uint32_t generation;
    uint8_t byte;
} debug_uart_rx_item_t;

static osMessageQueueId_t rx_queue;
static osThreadId_t tx_thread;
static uint8_t rx_byte;
static uint32_t rx_generation;
static uint32_t read_generation;
static debug_uart_status_t uart_status;
static uint8_t sample_buffer[192];
static volatile bool sample_active;
static uint16_t sample_length;
static uint32_t sample_started_ms;

bool debug_uart_start(void)
{
    tx_thread = osThreadGetId();
    rx_queue = osMessageQueueNew(DEBUG_UART_RX_QUEUE_LENGTH, sizeof(debug_uart_rx_item_t), NULL);
    if (rx_queue == NULL)
    {
        return false;
    }
    return HAL_UART_Receive_IT(&DEBUG_UART_HANDLE, &rx_byte, 1U) == HAL_OK;
}

bool debug_uart_read(uint8_t *byte, bool *data_lost, uint32_t timeout_ms)
{
    debug_uart_rx_item_t item;
    /* 工程启动时已核查 RTOS 为 1 ms 节拍。队列承担中断与任务间的数据交接。 */
    if (osMessageQueueGet(rx_queue, &item, NULL, timeout_ms) != osOK)
    {
        return false;
    }
    *byte = item.byte;
    *data_lost = item.generation != read_generation;
    read_generation = item.generation;
    return true;
}

bool debug_uart_write(const char *data, uint16_t length)
{
    taskENTER_CRITICAL();
    if (sample_active)
    {
        HAL_UART_AbortTransmit(&DEBUG_UART_HANDLE);
        sample_active = false;
        uart_status.sample_cancelled_count++;
    }
    taskEXIT_CRITICAL();
    /* 唯一调试任务串行发送；先清掉旧完成通知，避免误判本次传输已结束。 */
    osThreadFlagsClear(DEBUG_UART_TX_DONE);
    if (HAL_UART_Transmit_IT(&DEBUG_UART_HANDLE, (const uint8_t *)data, length) == HAL_OK)
    {
        const uint32_t flags =
            osThreadFlagsWait(DEBUG_UART_TX_DONE, osFlagsWaitAny, DEBUG_UART_TX_TIMEOUT_MS);
        if ((flags & osFlagsError) == 0U)
        {
            taskENTER_CRITICAL();
            uart_status.tx_bytes += length;
            taskEXIT_CRITICAL();
            return true;
        }
    }

    /* 超时后终止中断发送，调用方才可以复用栈上的响应缓冲区。 */
    HAL_UART_AbortTransmit(&DEBUG_UART_HANDLE);
    taskENTER_CRITICAL();
    uart_status.tx_error_count++;
    taskEXIT_CRITICAL();
    return false;
}

bool debug_uart_try_sample(const char *data, uint16_t length)
{
    if (length == 0U || length > sizeof(sample_buffer)) { return false; }
    if (sample_active)
    {
        if (osKernelGetTickCount() - sample_started_ms <= DEBUG_UART_TX_TIMEOUT_MS) { return false; }
        HAL_UART_AbortTransmit(&DEBUG_UART_HANDLE);
        sample_active = false;
        taskENTER_CRITICAL();
        uart_status.tx_error_count++;
        uart_status.sample_cancelled_count++;
        taskEXIT_CRITICAL();
    }
    memcpy(sample_buffer, data, length);
    sample_length = length;
    sample_active = true;
    sample_started_ms = osKernelGetTickCount();
    if (HAL_UART_Transmit_IT(&DEBUG_UART_HANDLE, sample_buffer, length) != HAL_OK)
    {
        sample_active = false;
        return false;
    }
    return true;
}

void debug_uart_get_status(debug_uart_status_t *status)
{
    taskENTER_CRITICAL();
    *status = uart_status;
    taskEXIT_CRITICAL();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (imu_uart_rx_callback(uart)) { return; }
    if (uart != &DEBUG_UART_HANDLE)
    {
        return;
    }

    /* HAL 先交付字节再回调错误；此时不能重挂接而清掉 ErrorCode。
     * 错误字节不进入队列，统一交给 ErrorCallback 恢复接收。
     */
    if (uart->ErrorCode != HAL_UART_ERROR_NONE)
    {
        return;
    }

    const debug_uart_rx_item_t item = {.generation = rx_generation, .byte = rx_byte};
    uart_status.rx_bytes++;
    if (osMessageQueuePut(rx_queue, &item, 0U, 0U) != osOK)
    {
        uart_status.rx_overflow_count++;
        rx_generation++;
    }
    if (HAL_UART_Receive_IT(uart, &rx_byte, 1U) != HAL_OK)
    {
        uart_status.rx_error_count++;
        rx_generation++;
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart == &DEBUG_UART_HANDLE)
    {
        if (sample_active)
        {
            sample_active = false;
            uart_status.tx_bytes += sample_length;
        }
        else
        {
            osThreadFlagsSet(tx_thread, DEBUG_UART_TX_DONE);
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (imu_uart_error_callback(uart)) { return; }
    if (uart != &DEBUG_UART_HANDLE)
    {
        return;
    }

    uart_status.rx_error_count++;
    rx_generation++;
    /* 中断接收的帧错误或溢出后重新挂接，恢复后的半条命令由控制台丢弃。 */
    HAL_UART_AbortReceive(uart);
    if (HAL_UART_Receive_IT(uart, &rx_byte, 1U) != HAL_OK)
    {
        uart_status.rx_error_count++;
    }
}
