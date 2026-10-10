#include "delay.h"
#include "stm32f1xx.h"

static uint32_t g_cycles_per_us = 8u;   /* 兜底值：8MHz HSI */

void delay_init(void)
{
    /* 打开 DWT/ITM 跟踪模块并使能 CYCCNT（CoreDebug->DEMCR.TRCENA = 1，DWT->CTRL.CYCCNTENA = 1） */
    CoreDebug->DEMCR |= 0x01000000UL;
    DWT->CYCCNT = 0UL;
    DWT->CTRL |= 0x00000001UL;

    g_cycles_per_us = SystemCoreClock / 1000000UL;
    if (g_cycles_per_us == 0UL) {
        g_cycles_per_us = 1UL;
    }
}

void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t wait = us * g_cycles_per_us;
    /* 单次最多延时约 2^32 个周期，够用；用有符号差值判断，天然处理回绕 */
    while ((uint32_t)(DWT->CYCCNT - start) < wait) {
        __NOP();
    }
}

void delay_ms(uint32_t ms)
{
    while (ms--) {
        delay_us(1000UL);
    }
}
