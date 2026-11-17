#include "time_us.h"

#include "main.h"

uint32_t time_us_now(void)
{
    /* TIM6 以 1 MHz 计数，更新中断待处理时补计该毫秒。 */
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    uint32_t milliseconds = HAL_GetTick();
    uint32_t microseconds = TIM6->CNT;
    if ((TIM6->SR & TIM_SR_UIF) != 0U)
    {
        milliseconds++;
        microseconds = TIM6->CNT;
    }
    if (primask == 0U) { __enable_irq(); }
    return milliseconds * 1000U + microseconds;
}
