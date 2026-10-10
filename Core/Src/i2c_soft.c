#include "i2c_soft.h"
#include "pins.h"
#include "delay.h"

/* 半周期(us)：2us ≈ 250kHz。若屏不亮/花屏，可改成 4~5 降速 */
#define SI2C_HALF_PERIOD_US 2u

/* --------------------------------------------------------------------------
 * 引脚电平控制：
 *   输出 0 -> 开漏输出模式 + ODR=0
 *   输出 1 -> 输入上拉模式 + ODR=1（释放总线，由内部上拉把电平拉高）
 * 这样即使模块上没焊上拉电阻也能通信。
 * -------------------------------------------------------------------------- */
static void pin_mode_od_low(GPIO_TypeDef *port, uint8_t pin)
{
    if (pin < 8u) {
        uint32_t m = 0xFU << (pin * 4u);
        port->CRL = (port->CRL & ~m) | (0x5UL << (pin * 4u));   /* MODE=01(10MHz) CNF=01(开漏) */
    } else {
        uint32_t m = 0xFU << ((pin - 8u) * 4u);
        port->CRH = (port->CRH & ~m) | (0x5UL << ((pin - 8u) * 4u));
    }
    port->ODR &= ~(1UL << pin);
}

static void pin_mode_pullup_high(GPIO_TypeDef *port, uint8_t pin)
{
    if (pin < 8u) {
        uint32_t m = 0xFU << (pin * 4u);
        port->CRL = (port->CRL & ~m) | (0x8UL << (pin * 4u));   /* MODE=00(输入) CNF=10(上/下拉) */
    } else {
        uint32_t m = 0xFU << ((pin - 8u) * 4u);
        port->CRH = (port->CRH & ~m) | (0x8UL << ((pin - 8u) * 4u));
    }
    port->ODR |= (1UL << pin);                                   /* ODR=1 选择上拉 */
}

static uint8_t pin_read(GPIO_TypeDef *port, uint8_t pin)
{
    return (uint8_t)((port->IDR >> pin) & 1UL);
}

#define SCL_LOW()  pin_mode_od_low(OLED_I2C_PORT, OLED_SCL_PIN)
#define SCL_HIGH() pin_mode_pullup_high(OLED_I2C_PORT, OLED_SCL_PIN)
#define SDA_LOW()  pin_mode_od_low(OLED_I2C_PORT, OLED_SDA_PIN)
#define SDA_HIGH() pin_mode_pullup_high(OLED_I2C_PORT, OLED_SDA_PIN)
#define SDA_READ() pin_read(OLED_I2C_PORT, OLED_SDA_PIN)

void si2c_init(void)
{
    /* 按 pins.h 里选定的端口开时钟（编译期常量，多余的分支会被优化掉） */
    if (OLED_I2C_PORT == GPIOA) { RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; }
    else if (OLED_I2C_PORT == GPIOB) { RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; }
    else if (OLED_I2C_PORT == GPIOC) { RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; }

    SCL_HIGH();
    SDA_HIGH();
    delay_us(10u);
}

void si2c_start(void)
{
    SDA_HIGH();
    SCL_HIGH();
    delay_us(SI2C_HALF_PERIOD_US);
    SDA_LOW();
    delay_us(SI2C_HALF_PERIOD_US);
    SCL_LOW();
}

void si2c_stop(void)
{
    SDA_LOW();
    delay_us(SI2C_HALF_PERIOD_US);
    SCL_HIGH();
    delay_us(SI2C_HALF_PERIOD_US);
    SDA_HIGH();
    delay_us(SI2C_HALF_PERIOD_US);
}

uint8_t si2c_write(uint8_t byte)
{
    uint8_t i;
    uint8_t ack;

    for (i = 0u; i < 8u; i++) {
        if (byte & 0x80u) {
            SDA_HIGH();
        } else {
            SDA_LOW();
        }
        delay_us(SI2C_HALF_PERIOD_US);
        SCL_HIGH();
        delay_us(SI2C_HALF_PERIOD_US);
        SCL_LOW();
        byte <<= 1;
    }

    /* 第 9 个时钟读 ACK */
    SDA_HIGH();
    delay_us(SI2C_HALF_PERIOD_US);
    SCL_HIGH();
    delay_us(SI2C_HALF_PERIOD_US);
    ack = (SDA_READ() == 0u) ? 1u : 0u;
    SCL_LOW();
    delay_us(SI2C_HALF_PERIOD_US);

    return ack;
}
