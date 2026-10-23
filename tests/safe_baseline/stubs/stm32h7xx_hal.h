#ifndef TEST_STM32_HAL_H
#define TEST_STM32_HAL_H

#include <stdint.h>

typedef struct
{
    uint32_t MODER;
    uint32_t ODR;
} GPIO_TypeDef;

typedef struct
{
    uint32_t Pin;
    uint32_t Mode;
    uint32_t Pull;
    uint32_t Speed;
} GPIO_InitTypeDef;

typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
extern GPIO_TypeDef test_gpio_a, test_gpio_b, test_gpio_c, test_gpio_d, test_gpio_e;
extern uint32_t test_clocks;
#define GPIOA (&test_gpio_a)
#define GPIOB (&test_gpio_b)
#define GPIOC (&test_gpio_c)
#define GPIOD (&test_gpio_d)
#define GPIOE (&test_gpio_e)
#define GPIO_MODE_OUTPUT_PP 1U
#define GPIO_PULLDOWN 2U
#define GPIO_SPEED_FREQ_LOW 0U
#define __HAL_RCC_GPIOA_CLK_ENABLE() (test_clocks |= 1U)
#define __HAL_RCC_GPIOB_CLK_ENABLE() (test_clocks |= 2U)
#define __HAL_RCC_GPIOE_CLK_ENABLE() (test_clocks |= 4U)

#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_6 (1U << 6)
#define GPIO_PIN_7 (1U << 7)
#define GPIO_PIN_8 (1U << 8)
#define GPIO_PIN_9 (1U << 9)
#define GPIO_PIN_11 (1U << 11)
#define GPIO_PIN_12 (1U << 12)
#define GPIO_PIN_13 (1U << 13)
#define GPIO_PIN_14 (1U << 14)

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pins, GPIO_PinState state);
void HAL_GPIO_Init(GPIO_TypeDef *port, const GPIO_InitTypeDef *config);

#endif
