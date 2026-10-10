#ifndef PINS_H
#define PINS_H

#include "stm32f1xx.h"

/* ==========================================================================
 * 接线定义（与 README.md 的接线表一一对应，改线只改这里）
 * ========================================================================== */

/* --- OLED 模块（SSD1306，I2C）--- */
#define OLED_I2C_PORT   GPIOB
#define OLED_SCL_PIN    6u    /* PB6  -> 模块 SCL(有的丝印 SDL/SCK) */
#define OLED_SDA_PIN    7u    /* PB7  -> 模块 SDA */

/* --- EC11 旋转编码器 AB 相：接 TIM2_CH1/CH2，固定 PA0/PA1 --- */
#define EC11_TIM        TIM2
#define EC11_PORT       GPIOA
#define EC11_A_PIN      0u    /* PA0 -> TRA */
#define EC11_B_PIN      1u    /* PA1 -> TRB */

/* --- 按键：内部上拉输入 --- */
#define KEY_PORT        GPIOA
#define KEY_PSH_PIN     2u    /* PA2 -> PSH（EC11 自带按键） */
#define KEY_CON_PIN     3u    /* PA3 -> CON（确认键） */
#define KEY_BACK_PIN    4u    /* PA4 -> BACK（返回键） */

/* 按键有效电平：模块按键通常是按下接地 -> 低有效(0)；若你的模块高有效改成 1 */
#define KEY_ACTIVE_LEVEL 0u

/* --- 上位机串口（USART1：PA9 TX / PA10 RX），接 USB-TTL 模块 ---
 * 用来接收 PC 发来的 OLED 位图帧，见 Core/Inc/uart_frame.h 的协议说明 */
#define HOST_UART        USART1
#define HOST_UART_PORT   GPIOA
#define HOST_UART_TX_PIN 9u    /* PA9  -> USB-TTL 的 RXD */
#define HOST_UART_RX_PIN 10u   /* PA10 -> USB-TTL 的 TXD */
#define HOST_UART_BAUD   921600UL
#define HOST_UART_IRQ    USART1_IRQHandler
#define HOST_UART_IRQn   USART1_IRQn
#define HOST_UART_DMA    DMA1_Channel5
#define HOST_UART_DMAEN  RCC_AHBENR_DMA1EN

#endif /* PINS_H */
