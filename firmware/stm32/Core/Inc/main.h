/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define M1_IN1_Pin GPIO_PIN_8
#define M1_IN1_GPIO_Port GPIOA
#define M1_IN2_Pin GPIO_PIN_11
#define M1_IN2_GPIO_Port GPIOA
#define M2_IN1_Pin GPIO_PIN_5
#define M2_IN1_GPIO_Port GPIOA
#define M2_IN2_Pin GPIO_PIN_3
#define M2_IN2_GPIO_Port GPIOB
#define M3_IN1_Pin GPIO_PIN_4
#define M3_IN1_GPIO_Port GPIOB
#define M3_IN2_Pin GPIO_PIN_5
#define M3_IN2_GPIO_Port GPIOB
#define M4_IN1_Pin GPIO_PIN_6
#define M4_IN1_GPIO_Port GPIOB
#define M4_IN2_Pin GPIO_PIN_7
#define M4_IN2_GPIO_Port GPIOB
#define M3_ENC_A_Pin GPIO_PIN_12
#define M3_ENC_A_GPIO_Port GPIOD
#define M3_ENC_B_Pin GPIO_PIN_13
#define M3_ENC_B_GPIO_Port GPIOD
#define M4_ENC_A_Pin GPIO_PIN_6
#define M4_ENC_A_GPIO_Port GPIOC
#define M4_ENC_B_Pin GPIO_PIN_7
#define M4_ENC_B_GPIO_Port GPIOC
#define Servo1_Pin GPIO_PIN_9
#define Servo1_GPIO_Port GPIOE
#define Servo2_Pin GPIO_PIN_11
#define Servo2_GPIO_Port GPIOE
#define Servo3_Pin GPIO_PIN_13
#define Servo3_GPIO_Port GPIOE
#define Servo4_Pin GPIO_PIN_14
#define Servo4_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
