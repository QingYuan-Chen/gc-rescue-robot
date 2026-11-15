#ifndef ROBOT_IMU_H
#define ROBOT_IMU_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int16_t accel[3];
    int16_t gyro[3];
    int16_t angle[3];
    uint32_t accel_ms, gyro_ms, angle_ms;
    uint32_t accel_frames, gyro_frames, angle_frames;
    uint32_t checksum_errors, uart_errors;
    bool receiving;
} imu_status_t;

bool imu_start(void);
void imu_get_status(imu_status_t *status);
bool imu_uart_rx_callback(UART_HandleTypeDef *uart);
bool imu_uart_error_callback(UART_HandleTypeDef *uart);
void imu_uart_irq(void);

#endif
