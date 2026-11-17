#ifndef ROBOT_TIM_H
#define ROBOT_TIM_H

#include "main.h"

extern TIM_HandleTypeDef htim1, htim2, htim3, htim5;
void MX_TIM1_Init(void);
void MX_TIM2_Init(void);
void MX_TIM3_Init(void);
void MX_TIM5_Init(void);

#endif
