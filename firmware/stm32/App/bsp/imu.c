#include "imu.h"

#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define IMU_BAUD_RATE 115200U
#define IMU_FRAME_SIZE 11U

static UART_HandleTypeDef imu_uart;
static uint8_t rx_byte, frame[IMU_FRAME_SIZE], frame_size;
static imu_status_t latest;

static int16_t read_i16(const uint8_t *bytes)
{
    const uint16_t value = (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
    return (int16_t)value;
}

static void parse_byte(uint8_t byte)
{
    if (frame_size == 0U && byte != 0x55U) { return; }
    frame[frame_size++] = byte;
    if (frame_size < IMU_FRAME_SIZE) { return; }

    uint8_t checksum = 0U;
    for (unsigned i = 0; i < IMU_FRAME_SIZE - 1U; i++) { checksum += frame[i]; }
    if (checksum == frame[10])
    {
        int16_t *values = NULL;
        uint32_t *timestamp = NULL, *count = NULL;
        if (frame[1] == 0x51U)
        {
            values = latest.accel; timestamp = &latest.accel_ms; count = &latest.accel_frames;
        }
        else if (frame[1] == 0x52U)
        {
            values = latest.gyro; timestamp = &latest.gyro_ms; count = &latest.gyro_frames;
        }
        else if (frame[1] == 0x53U)
        {
            values = latest.angle; timestamp = &latest.angle_ms; count = &latest.angle_frames;
        }
        if (values != NULL)
        {
            for (unsigned i = 0; i < 3U; i++) { values[i] = read_i16(&frame[2U + 2U * i]); }
            *timestamp = HAL_GetTick();
            (*count)++;
        }
        frame_size = 0U;
        return;
    }

    latest.checksum_errors++;
    /* 校验失败时保留可能的下一帧头，逐字节重新同步。 */
    unsigned next = 1U;
    while (next < IMU_FRAME_SIZE && frame[next] != 0x55U) { next++; }
    frame_size = (uint8_t)(IMU_FRAME_SIZE - next);
    memmove(frame, &frame[next], frame_size);
}

bool imu_start(void)
{
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_USART3_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &gpio);

    imu_uart.Instance = USART3;
    imu_uart.Init.BaudRate = IMU_BAUD_RATE;
    imu_uart.Init.WordLength = UART_WORDLENGTH_8B;
    imu_uart.Init.StopBits = UART_STOPBITS_1;
    imu_uart.Init.Parity = UART_PARITY_NONE;
    imu_uart.Init.Mode = UART_MODE_TX_RX;
    imu_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    imu_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    imu_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    imu_uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    imu_uart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&imu_uart) != HAL_OK ||
        HAL_UARTEx_DisableFifoMode(&imu_uart) != HAL_OK) { return false; }
    HAL_NVIC_SetPriority(USART3_IRQn, 7U, 0U);
    if (HAL_UART_Receive_IT(&imu_uart, &rx_byte, 1U) != HAL_OK) { return false; }
    latest.receiving = true;
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    return true;
}

void imu_get_status(imu_status_t *status)
{
    taskENTER_CRITICAL();
    *status = latest;
    taskEXIT_CRITICAL();
}

void imu_uart_irq(void) { HAL_UART_IRQHandler(&imu_uart); }

bool imu_uart_rx_callback(UART_HandleTypeDef *uart)
{
    if (uart != &imu_uart) { return false; }
    if (uart->ErrorCode != HAL_UART_ERROR_NONE) { return true; }
    parse_byte(rx_byte);
    if (HAL_UART_Receive_IT(uart, &rx_byte, 1U) != HAL_OK)
    {
        latest.uart_errors++;
        latest.receiving = false;
    }
    return true;
}

bool imu_uart_error_callback(UART_HandleTypeDef *uart)
{
    if (uart != &imu_uart) { return false; }
    latest.uart_errors++;
    frame_size = 0U;
    HAL_UART_AbortReceive(uart);
    latest.receiving = HAL_UART_Receive_IT(uart, &rx_byte, 1U) == HAL_OK;
    return true;
}
