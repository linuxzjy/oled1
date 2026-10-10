#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

/* 基于 Cortex-M3 DWT 周期计数器的微秒/毫秒级阻塞延时 */
void delay_init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

#endif /* DELAY_H */
