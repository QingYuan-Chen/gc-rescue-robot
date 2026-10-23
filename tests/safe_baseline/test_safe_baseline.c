#include "bsp/board_outputs.h"
#include "bsp/debug_uart.h"
#include "debug/debug_console.h"
#include "runtime/runtime_monitor.h"
#include "main.h"

#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GPIO_TypeDef test_gpio_a, test_gpio_b, test_gpio_c, test_gpio_d, test_gpio_e;
uint32_t test_clocks;
static unsigned writes, initializations;
static jmp_buf console_finished;
static const char *input;
static char output[8192];
static size_t output_length;

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pins, GPIO_PinState state)
{
    assert(test_clocks == 7U);
    assert(state == GPIO_PIN_RESET);
    port->ODR &= ~pins;
    writes++;
}

void HAL_GPIO_Init(GPIO_TypeDef *port, const GPIO_InitTypeDef *config)
{
    /* 即使锁存值初始全高，也必须在输出模式生效前清零。 */
    assert((port->ODR & config->Pin) == 0U);
    assert(config->Mode == GPIO_MODE_OUTPUT_PP);
    assert(config->Pull == GPIO_PULLDOWN);
    for (unsigned pin = 0; pin < 16; pin++)
    {
        if (config->Pin & (1U << pin))
        {
            port->MODER = (port->MODER & ~(3U << (pin * 2))) | (1U << (pin * 2));
        }
    }
    initializations++;
}

void Error_Handler(void) { abort(); }
uint32_t osKernelGetTickCount(void) { return 200U; }
void *osThreadGetId(void) { return NULL; }
uint32_t osThreadGetStackSpace(void *thread) { (void)thread; return 512U; }
bool debug_uart_start(void) { return true; }
bool debug_uart_read(uint8_t *byte, bool *lost, uint32_t timeout)
{
    (void)timeout;
    if (*input == '\0')
    {
        longjmp(console_finished, 1);
    }
    *byte = (uint8_t)*input++;
    *lost = false;
    return true;
}
bool debug_uart_write(const char *text, uint16_t length)
{
    assert(output_length + length < sizeof(output));
    memcpy(output + output_length, text, length);
    output_length += length;
    output[output_length] = '\0';
    return true;
}
void debug_uart_get_status(debug_uart_status_t *status) { memset(status, 0, sizeof(*status)); }
void runtime_monitor_get_status(runtime_monitor_status_t *status)
{
    memset(status, 0, sizeof(*status));
    status->period_ticks = 10U;
}

static void check_pin_mapping(void)
{
    /* 独立按新版网表核对，不沿用旧 C/D 对应关系。 */
    assert(M4_IN1_GPIO_Port == GPIOB && M4_IN1_Pin == (1U << 6));
    assert(M4_IN2_GPIO_Port == GPIOB && M4_IN2_Pin == (1U << 7));
    assert(M3_IN1_GPIO_Port == GPIOB && M3_IN1_Pin == (1U << 4));
    assert(M3_IN2_GPIO_Port == GPIOB && M3_IN2_Pin == (1U << 5));
    assert(M4_ENC_A_Pin == (1U << 6) && M4_ENC_B_Pin == (1U << 7));
    assert(M3_ENC_A_Pin == (1U << 12) && M3_ENC_B_Pin == (1U << 13));
    assert(M4_ENC_A_GPIO_Port == GPIOC && M4_ENC_B_GPIO_Port == GPIOC);
    assert(M3_ENC_A_GPIO_Port == GPIOD && M3_ENC_B_GPIO_Port == GPIOD);
}

static void check_outputs(void)
{
    test_gpio_a = test_gpio_b = test_gpio_e = (GPIO_TypeDef){0xAAAAAAAAU, 0xFFFFU};
    assert(!board_outputs_are_disabled());
    board_outputs_disable();
    assert(writes == 3 && initializations == 3);
    assert(test_gpio_a.ODR == (0xFFFFU & ~((1U << 5) | (1U << 8) | (1U << 11))));
    assert(test_gpio_b.ODR == (0xFFFFU & ~0xF8U));
    assert(test_gpio_e.ODR == (0xFFFFU & ~0x6A00U));
    assert(board_outputs_are_disabled());

    GPIO_TypeDef *ports[] = {GPIOA, GPIOB, GPIOE};
    const unsigned masks[] = {(1U << 5) | (1U << 8) | (1U << 11), 0xF8U, 0x6A00U};
    for (unsigned p = 0; p < 3; p++)
    {
        for (unsigned pin = 0; pin < 16; pin++)
        {
            if ((masks[p] & (1U << pin)) == 0U) { continue; }
            ports[p]->ODR |= 1U << pin;
            assert(!board_outputs_are_disabled());
            board_outputs_disable();
            ports[p]->MODER |= 3U << (pin * 2);
            assert(!board_outputs_are_disabled());
            board_outputs_disable();
        }
    }
    assert(board_outputs_are_disabled());
}

static void check_console(void)
{
    input = "help\rping\rstatus\rget report_ms\rset report_ms 0\r"
            "wheel left 100 1000\rwheel speed both 10 10 1000\rwheel stop\r"
            "pid apply left\rvofa on\r";
    const unsigned previous_writes = writes;
    if (setjmp(console_finished) == 0) { debug_console_run(); }
    assert(strstr(output, "OK pong\r\n") != NULL);
    assert(strstr(output, "profile=tb67h450-safe left=M4 right=M3 outputs_low=1") != NULL);
    assert(strstr(output, "motor_control=0 encoders=0\r\n") != NULL);
    unsigned rejected = 0U;
    for (const char *p = output; (p = strstr(p, "ERR unknown command; use help\r\n")); p++)
    {
        rejected++;
    }
    assert(rejected == 5U);
    assert(writes == previous_writes);
    assert(board_outputs_are_disabled());
}

int main(void)
{
    check_pin_mapping();
    check_outputs();
    check_console();
    puts("PASS: pin mapping, low-before-output, readback fault detection, old command rejection");
    return 0;
}
