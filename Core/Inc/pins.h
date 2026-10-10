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

#endif /* PINS_H */
