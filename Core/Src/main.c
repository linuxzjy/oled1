/**
 * STM32F103C8T6 + SSD1306(128x64, I2C) + EC11 旋转编码器
 * 功能：小猫动画 + 实时时钟（RTC），EC11/按键可翻页、校准时间
 *
 * 接线见 README.md，改引脚看 Core/Inc/pins.h
 */
#include "stm32f1xx.h"
#include "delay.h"
#include "i2c_soft.h"
#include "ssd1306.h"
#include "rtc_time.h"
#include "ec11.h"
#include "keys.h"
#include "ui.h"

static volatile uint32_t g_ms = 0;
static volatile uint8_t  g_tick = 0u;

void SysTick_Handler(void)
{
    g_ms++;
    g_tick = 1u;
    keys_scan_1ms();          /* 1ms 扫描按键（含消抖） */
}

/* HSE 8MHz * 9 = 72MHz；外部晶振没起振就退回内部 HSI 8MHz */
static void sysclock_init(void)
{
    uint32_t t = 0u;

    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) {
        if (++t > 1000000UL) { break; }
    }

    if (RCC->CR & RCC_CR_HSERDY) {
        FLASH->ACR |= FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;      /* 72MHz 需要 2 个等待周期 */
        RCC->CFGR |= RCC_CFGR_HPRE_DIV1;                            /* AHB  = 72MHz */
        RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;                           /* APB2 = 72MHz */
        RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;                           /* APB1 = 36MHz */
        RCC->CFGR |= RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL9;           /* 8MHz * 9 */
        RCC->CR |= RCC_CR_PLLON;
        while (!(RCC->CR & RCC_CR_PLLRDY)) { }
        RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { }
        SystemCoreClock = 72000000UL;
    } else {
        RCC->CR &= ~RCC_CR_HSEON;
        SystemCoreClock = 8000000UL;
    }
}

/* 板载 LED(PC13，低电平点亮)：正常 = 慢闪，找不到屏幕 = 快闪 */
static void led_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPCEN;
    GPIOC->CRH &= ~(GPIO_CRH_MODE13 | GPIO_CRH_CNF13);
    GPIOC->CRH |= GPIO_CRH_MODE13_1;
    GPIOC->ODR |= GPIO_ODR_ODR13;
}

static void led_toggle(void)
{
    GPIOC->ODR ^= GPIO_ODR_ODR13;
}

int main(void)
{
    uint8_t   oled_ok;
    uint32_t  last_check = 0u;
    uint32_t  last_led = 0u;
    uint32_t  led_period = 500u;
    datetime_t dt;

    sysclock_init();
    delay_init();
    SysTick_Config(SystemCoreClock / 1000UL);      /* 1ms 节拍 */
    led_init();

    si2c_init();
    oled_ok = ssd1306_init();

    rtc_init();
    ec11_init();
    keys_init();
    ui_init();

    if (!oled_ok) {
        led_period = 100u;                          /* 屏幕没接好：快闪报警 */
    }

    for (;;) {
        uint32_t now;
        int32_t  delta;
        key_id_t key;

        if (g_tick) {
            g_tick = 0u;

            delta = ec11_take_delta();
            if (delta != 0) {
                ui_encoder(delta);
            }

            key = keys_take_event();
            if (key != KEY_NONE) {
                ui_key(key);
            }
        }

        now = g_ms;

        /* 100ms 检查一次：时间变了 / 有输入 / 小猫眨眼才重画，避免无谓刷屏 */
        if ((uint32_t)(now - last_check) >= 100u) {
            last_check = now;

            rtc_get_datetime(&dt);
            if (ui_need_redraw(&dt, now)) {
                ui_draw(&dt, now);
                ssd1306_refresh();
            }
        }

        if ((uint32_t)(now - last_led) >= led_period) {
            last_led = now;
            led_toggle();
        }
    }
}
