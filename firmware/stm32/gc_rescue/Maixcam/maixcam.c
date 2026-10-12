#include "maixcam.h"

#define maixcam huart2
#define BUFFER_SIZE  100
volatile uint8_t maixcam_rx_buf[BUFFER_SIZE] = {0};
volatile uint8_t maixcam_rx_len = 0;

void Maixcam_init(void){
  __HAL_UART_ENABLE_IT(&maixcam, UART_IT_IDLE);
	HAL_UART_Receive_DMA(&maixcam,maixcam_rx_buf,BUFFER_SIZE);
}

// 原实现经 USART3 发送单字节模式值；此处保留现有行为，不视为新协议。
void Set_Vision_Mode(uint8_t mode){
    uint8_t data = mode;
    my_car.track_mode = mode;
    HAL_UART_Transmit(&huart3,&data,1,0xfff);
}

extern DMA_HandleTypeDef hdma_usart2_rx;

void USART2_IRQHandler(void){
	uint32_t flag_idle = 0;
	
	flag_idle = __HAL_UART_GET_FLAG(&maixcam,UART_FLAG_IDLE);
	if((flag_idle != RESET))
	{ 
		__HAL_UART_CLEAR_IDLEFLAG(&maixcam);
		HAL_UART_DMAStop(&maixcam);
		uint32_t temp = __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
		maixcam_rx_len  = BUFFER_SIZE - temp;
		// 原接收格式为 6 字节：队伍、模式、数量、危险标志、x、y。
		if(maixcam_rx_len == 6){
			if(maixcam_rx_buf[0] == 0x01 || maixcam_rx_buf[0] == 0x02){
				if(my_car.vision_manager.team  == 0){ // 仅在队伍尚未设置时记录本次队伍值
                    my_car.vision_manager.team = maixcam_rx_buf[0];
				}
                my_car.vision_manager.mode = maixcam_rx_buf[1];
                my_car.vision_manager.bolls_num = maixcam_rx_buf[2];
                my_car.vision_manager.have_danger = maixcam_rx_buf[3];
                my_car.vision_manager.x = maixcam_rx_buf[4];
                my_car.vision_manager.y = maixcam_rx_buf[5];
			}
		}
        memset(maixcam_rx_buf,0,maixcam_rx_len);
        maixcam_rx_len = 0;
	}
	HAL_UART_Receive_DMA(&maixcam,maixcam_rx_buf,BUFFER_SIZE);
	HAL_UART_IRQHandler(&maixcam);
}
