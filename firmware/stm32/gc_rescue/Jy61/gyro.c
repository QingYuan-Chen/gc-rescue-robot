#include "gyro.h"

#define gyro huart3
#define BUFFER_SIZE  100
volatile uint8_t imu_rx_buf[BUFFER_SIZE] = {0};
volatile uint8_t imu_rx_len = 0;

void gyro_init(void)
{	
	__HAL_UART_ENABLE_IT(&gyro, UART_IT_IDLE);
	HAL_UART_Receive_DMA(&gyro,imu_rx_buf,BUFFER_SIZE);
}

extern DMA_HandleTypeDef hdma_usart3_rx;

void USART3_IRQHandler(void){
	uint32_t flag_idle = 0;
	
	flag_idle = __HAL_UART_GET_FLAG(&gyro,UART_FLAG_IDLE); 
	if((flag_idle != RESET))
	{ 
		__HAL_UART_CLEAR_IDLEFLAG(&gyro);
		HAL_UART_DMAStop(&gyro); 
		uint32_t temp = __HAL_DMA_GET_COUNTER(&hdma_usart3_rx);   
		imu_rx_len = BUFFER_SIZE - temp; 
		
		if(imu_rx_buf[0] == 0x55){
			if(imu_rx_buf[1] == 0x53){
				uint8_t sum = 0;
				for (int i=0; i<10; i++)
					sum += imu_rx_buf[i];
				if (sum == imu_rx_buf[10]){
						float yaw  = 180.0*(short)((imu_rx_buf[29]<<8)|imu_rx_buf[28])/32768.0f;
						if(yaw>360) yaw -= 360;
						else if(yaw<0) yaw += 360;
						my_car.angle_manager.yaw  = yaw;
				}
			}
		}
        memset(imu_rx_buf,0,imu_rx_len);
		imu_rx_len = 0;
	}
	HAL_UART_Receive_DMA(&gyro,imu_rx_buf,BUFFER_SIZE);
	HAL_UART_IRQHandler(&gyro); 	
}
