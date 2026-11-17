/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32h7xx_hal_timebase_tim.c
  * @brief   HAL time base based on the hardware TIM.
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

/* HAL 毫秒时基独占 TIM6，将 TIM17 留给左电机 IN2。 */
#include "stm32h7xx_hal.h"

TIM_HandleTypeDef htim6;

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef clocks;
  uint32_t latency;
  if (TickPriority >= (1UL << __NVIC_PRIO_BITS)) { return HAL_ERROR; }
  __HAL_RCC_TIM6_CLK_ENABLE();
  /* HAL_Init 和系统时钟切换都会调用，先停止旧时基再重配。 */
  TIM6->CR1 = 0U;
  TIM6->DIER = 0U;
  HAL_RCC_GetClockConfig(&clocks, &latency);
  /* 当前 TIMPRE 使用复位配置：APB1 不分频时同频，其余当前配置为 PCLK1 的两倍。 */
  uint32_t timer_clock = HAL_RCC_GetPCLK1Freq();
  if (clocks.APB1CLKDivider != RCC_APB1_DIV1) { timer_clock *= 2U; }
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = timer_clock / 1000000U - 1U;
  htim6.Init.Period = 999U;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK) { return HAL_ERROR; }
  /* 初始化会产生更新标志，启用 IRQ 前清除，避免将配置事件多算成一次毫秒 tick。 */
  __HAL_TIM_CLEAR_FLAG(&htim6, TIM_FLAG_UPDATE);
  HAL_NVIC_SetPriority(TIM6_DAC_IRQn, TickPriority, 0U);
  HAL_NVIC_ClearPendingIRQ(TIM6_DAC_IRQn);
  HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
  uwTickPrio = TickPriority;
  return HAL_TIM_Base_Start_IT(&htim6);
}

void HAL_SuspendTick(void)
{
  __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

void HAL_ResumeTick(void)
{
  __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}
