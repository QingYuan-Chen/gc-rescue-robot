#ifndef TEST_WHEEL_HAL_H
#define TEST_WHEEL_HAL_H
#include "../../safe_baseline/stubs/stm32h7xx_hal.h"

typedef struct
{
    uint32_t CNT, CCR1, CCR2, CCR4, EGR, CR1, DIER, SR;
} TIM_TypeDef;
typedef struct
{
    uint32_t Prescaler, CounterMode, Period, ClockDivision, AutoReloadPreload;
} TIM_Base_InitTypeDef;
typedef struct
{
    TIM_TypeDef *Instance;
    TIM_Base_InitTypeDef Init;
} TIM_HandleTypeDef;
typedef struct
{
    uint32_t OCMode, Pulse, OCPolarity, OCNPolarity, OCFastMode, OCIdleState, OCNIdleState;
} TIM_OC_InitTypeDef;
typedef struct
{
    uint32_t EncoderMode, IC1Polarity, IC1Selection, IC1Prescaler, IC1Filter;
    uint32_t IC2Polarity, IC2Selection, IC2Prescaler, IC2Filter;
} TIM_Encoder_InitTypeDef;
typedef enum
{
    HAL_OK,
    HAL_ERROR
} HAL_StatusTypeDef;
typedef struct
{
    uint32_t APB1CLKDivider;
} RCC_ClkInitTypeDef;
extern TIM_TypeDef test_tim1, test_tim2, test_tim5, test_tim3, test_tim4, test_tim6, test_tim8, test_tim16, test_tim17;
extern uint32_t uwTickPrio;
#define TIM1 (&test_tim1)
#define TIM2 (&test_tim2)
#define TIM5 (&test_tim5)
#define TIM3 (&test_tim3)
#define TIM4 (&test_tim4)
#define TIM6 (&test_tim6)
#define TIM8 (&test_tim8)
#define TIM16 (&test_tim16)
#define TIM17 (&test_tim17)
#define TIM_CHANNEL_1 0U
#define TIM_CHANNEL_2 4U
#define TIM_CHANNEL_4 12U
#define TIM_CHANNEL_ALL 60U
#define TIM_COUNTERMODE_UP 0U
#define TIM_CLOCKDIVISION_DIV1 0U
#define TIM_AUTORELOAD_PRELOAD_ENABLE 128U
#define TIM_AUTORELOAD_PRELOAD_DISABLE 0U
#define TIM_OCMODE_PWM1 96U
#define TIM_OCPOLARITY_HIGH 0U
#define TIM_OCNPOLARITY_HIGH 0U
#define TIM_OCFAST_DISABLE 0U
#define TIM_OCIDLESTATE_RESET 0U
#define TIM_OCNIDLESTATE_RESET 0U
#define TIM_ENCODERMODE_TI12 3U
#define TIM_ICPOLARITY_RISING 0U
#define TIM_ICSELECTION_DIRECTTI 1U
#define TIM_ICPSC_DIV1 0U
#define TIM_EGR_UG 1U
#define TIM_FLAG_UPDATE 1U
#define TIM_IT_UPDATE 1U
#define TIM6_DAC_IRQn 54
#define __NVIC_PRIO_BITS 4U
#define RCC_APB1_DIV1 0U
#define GPIO_PIN_0 1U
#define GPIO_PIN_1 2U
#define GPIO_MODE_AF_PP 2U
#define GPIO_PULLUP 1U
#define GPIO_AF1_TIM1 1U
#define GPIO_AF2_TIM5 2U
#define __HAL_RCC_TIM1_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM2_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM5_CLK_ENABLE() ((void)0)
#define GPIO_AF1_TIM16 1U
#define GPIO_AF2_TIM3 2U
#define GPIO_AF2_TIM4 2U
#define GPIO_AF3_TIM8 3U
#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_GPIOD_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM3_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM4_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM6_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM8_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM16_CLK_ENABLE() ((void)0)
#define __HAL_RCC_TIM17_CLK_ENABLE() ((void)0)
#define __HAL_TIM_SET_COUNTER(h, v) ((h)->Instance->CNT = (v))
#define __HAL_TIM_GET_COUNTER(h) ((h)->Instance->CNT)
#define __HAL_TIM_SET_COMPARE(h, c, v)                                                             \
    ((c) == TIM_CHANNEL_1 ? ((h)->Instance->CCR1 = (v)) : (c) == TIM_CHANNEL_2 ? ((h)->Instance->CCR2 = (v)) : ((h)->Instance->CCR4 = (v)))
#define __HAL_TIM_CLEAR_FLAG(h, f) ((h)->Instance->SR &= ~(f))
#define __HAL_TIM_DISABLE_IT(h, f) ((h)->Instance->DIER &= ~(f))
#define __HAL_TIM_ENABLE_IT(h, f) ((h)->Instance->DIER |= (f))

HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *timer);
HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *timer, const TIM_OC_InitTypeDef *oc,
                                            uint32_t channel);
HAL_StatusTypeDef HAL_TIM_Encoder_Init(TIM_HandleTypeDef *timer, const TIM_Encoder_InitTypeDef *ic);
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *timer, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_Encoder_Stop(TIM_HandleTypeDef *timer, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *timer, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *timer, uint32_t channel);
HAL_StatusTypeDef HAL_TIMEx_PWMN_Start(TIM_HandleTypeDef *timer, uint32_t channel);
HAL_StatusTypeDef HAL_TIMEx_PWMN_Stop(TIM_HandleTypeDef *timer, uint32_t channel);
HAL_StatusTypeDef HAL_TIM_Base_Init(TIM_HandleTypeDef *timer);
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *timer);
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *timer);
void HAL_TIM_Encoder_MspInit(TIM_HandleTypeDef *timer);
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *clocks, uint32_t *latency);
uint32_t HAL_RCC_GetPCLK1Freq(void);
void HAL_NVIC_SetPriority(int irq, uint32_t preempt, uint32_t sub);
void HAL_NVIC_ClearPendingIRQ(int irq);
void HAL_NVIC_EnableIRQ(int irq);
HAL_StatusTypeDef HAL_InitTick(uint32_t priority);
void HAL_SuspendTick(void);
void HAL_ResumeTick(void);
#endif
