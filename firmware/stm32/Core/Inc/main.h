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
#define PWMA_Pin GPIO_PIN_5
#define PWMA_GPIO_Port GPIOE
#define PWMB_Pin GPIO_PIN_6
#define PWMB_GPIO_Port GPIOE
#define EncoderB1_Pin GPIO_PIN_0
#define EncoderB1_GPIO_Port GPIOA
#define EncoderB2_Pin GPIO_PIN_1
#define EncoderB2_GPIO_Port GPIOA
#define maixcam_TX_Pin GPIO_PIN_2
#define maixcam_TX_GPIO_Port GPIOA
#define maixcam_RX_Pin GPIO_PIN_3
#define maixcam_RX_GPIO_Port GPIOA
#define EncoderA1_Pin GPIO_PIN_6
#define EncoderA1_GPIO_Port GPIOA
#define EncoderA2_Pin GPIO_PIN_7
#define EncoderA2_GPIO_Port GPIOA
#define BIN2_Pin GPIO_PIN_8
#define BIN2_GPIO_Port GPIOE
#define Servo1_Pin GPIO_PIN_9
#define Servo1_GPIO_Port GPIOE
#define BIN1_Pin GPIO_PIN_10
#define BIN1_GPIO_Port GPIOE
#define Servo2_Pin GPIO_PIN_11
#define Servo2_GPIO_Port GPIOE
#define AIN1_Pin GPIO_PIN_12
#define AIN1_GPIO_Port GPIOE
#define Servo3_Pin GPIO_PIN_13
#define Servo3_GPIO_Port GPIOE
#define Servo4_Pin GPIO_PIN_14
#define Servo4_GPIO_Port GPIOE
#define AIN2_Pin GPIO_PIN_10
#define AIN2_GPIO_Port GPIOB
#define PWMD_Pin GPIO_PIN_14
#define PWMD_GPIO_Port GPIOB
#define PWMC_Pin GPIO_PIN_15
#define PWMC_GPIO_Port GPIOB
#define JY60_TX_Pin GPIO_PIN_8
#define JY60_TX_GPIO_Port GPIOD
#define JY60_RX_Pin GPIO_PIN_9
#define JY60_RX_GPIO_Port GPIOD
#define EncoderC1_Pin GPIO_PIN_12
#define EncoderC1_GPIO_Port GPIOD
#define EncoderC2_Pin GPIO_PIN_13
#define EncoderC2_GPIO_Port GPIOD
#define EncoderD1_Pin GPIO_PIN_6
#define EncoderD1_GPIO_Port GPIOC
#define EncoderD2_Pin GPIO_PIN_7
#define EncoderD2_GPIO_Port GPIOC
#define HC05_TX_Pin GPIO_PIN_9
#define HC05_TX_GPIO_Port GPIOA
#define HC05_RX_Pin GPIO_PIN_10
#define HC05_RX_GPIO_Port GPIOA
#define DIN2_Pin GPIO_PIN_1
#define DIN2_GPIO_Port GPIOD
#define DIN1_Pin GPIO_PIN_2
#define DIN1_GPIO_Port GPIOD
#define CIN1_Pin GPIO_PIN_3
#define CIN1_GPIO_Port GPIOD
#define CIN2_Pin GPIO_PIN_4
#define CIN2_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
