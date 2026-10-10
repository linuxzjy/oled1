#include "keys.h"
#include "pins.h"
#include "stm32f1xx.h"

#define DEBOUNCE_MS 25u

typedef struct {
    uint8_t pin;
    uint8_t pressed;     /* 消抖后的稳定状态 */
    uint8_t cnt;
} key_state_t;

static key_state_t g_keys[] = {
    { KEY_PSH_PIN,  0u, 0u },
    { KEY_CON_PIN,  0u, 0u },
    { KEY_BACK_PIN, 0u, 0u },
};

static const key_id_t g_key_id[] = { KEY_PSH, KEY_CON, KEY_BACK };

static volatile uint8_t g_event = (uint8_t)KEY_NONE;

void keys_init(void)
{
    uint32_t i;

    if (KEY_PORT == GPIOA)      { RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; }
    else if (KEY_PORT == GPIOB) { RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; }
    else if (KEY_PORT == GPIOC) { RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; }

    for (i = 0u; i < (sizeof(g_keys) / sizeof(g_keys[0])); i++) {
        uint8_t pin = g_keys[i].pin;
        if (pin < 8u) {
            KEY_PORT->CRL &= ~(0xFU << (pin * 4u));
            KEY_PORT->CRL |= (0x8UL << (pin * 4u));     /* 输入 + 上/下拉 */
        } else {
            KEY_PORT->CRH &= ~(0xFU << ((pin - 8u) * 4u));
            KEY_PORT->CRH |= (0x8UL << ((pin - 8u) * 4u));
        }
        KEY_PORT->ODR |= (1UL << pin);                   /* ODR=1 -> 上拉 */
    }
}

void keys_scan_1ms(void)
{
    uint32_t i;

    for (i = 0u; i < (sizeof(g_keys) / sizeof(g_keys[0])); i++) {
        uint8_t pin = g_keys[i].pin;
        uint8_t raw = (uint8_t)((KEY_PORT->IDR >> pin) & 1UL);
        uint8_t pressed = (raw == (uint8_t)KEY_ACTIVE_LEVEL) ? 1u : 0u;

        if (pressed != g_keys[i].pressed) {
            g_keys[i].cnt++;
            if (g_keys[i].cnt >= DEBOUNCE_MS) {
                g_keys[i].cnt = 0u;
                g_keys[i].pressed = pressed;
                if (pressed) {
                    g_event = (uint8_t)g_key_id[i];     /* 只在按下的那一刻产生事件 */
                }
            }
        } else {
            g_keys[i].cnt = 0u;
        }
    }
}

key_id_t keys_take_event(void)
{
    key_id_t e;

    __disable_irq();
    e = (key_id_t)g_event;
    g_event = (uint8_t)KEY_NONE;
    __enable_irq();

    return e;
}
