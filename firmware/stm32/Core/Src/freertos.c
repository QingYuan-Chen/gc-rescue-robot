/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "robot_app.h"
#include "debug/debug_console.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for robot_task */
osThreadId_t robot_taskHandle;
const osThreadAttr_t robot_task_attributes = {
  .name = "robot_task",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};

/* Definitions for debug_task */
osThreadId_t debug_taskHandle;
const osThreadAttr_t debug_task_attributes = {
  .name = "debug_task",
  .stack_size = 768 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void robot_task_entry(void *argument);
void debug_task_entry(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  /* 基础时基固定为 1 ms 的 RTOS 节拍，初始化失败时不创建应用任务。 */
  if (osKernelGetState() != osKernelReady || osKernelGetTickFreq() != 1000U)
  {
    Error_Handler();
  }
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of robot_task */
  robot_taskHandle = osThreadNew(robot_task_entry, NULL, &robot_task_attributes);

  /* creation of debug_task */
  debug_taskHandle = osThreadNew(debug_task_entry, NULL, &debug_task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  if (robot_taskHandle == NULL || debug_taskHandle == NULL)
  {
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_robot_task_entry */
/**
  * @brief  Function implementing the robot_task thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_robot_task_entry */
void robot_task_entry(void *argument)
{
  /* USER CODE BEGIN robot_task_entry */
  (void)argument;
  robot_app_run();
  /* USER CODE END robot_task_entry */
}

/* USER CODE BEGIN Header_debug_task_entry */
/**
  * @brief  Function implementing the debug_task thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_debug_task_entry */
void debug_task_entry(void *argument)
{
  /* USER CODE BEGIN debug_task_entry */
  (void)argument;
  debug_console_run();
  /* USER CODE END debug_task_entry */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

