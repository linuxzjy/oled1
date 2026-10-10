#ifndef I2C_SOFT_H
#define I2C_SOFT_H

#include <stdint.h>

/*
 * 软件 I2C（位bang）。
 * 之所以不用硬件 I2C：OLED 模块有的没焊上拉电阻、有的地址/时序略有差异，
 * 软件 I2C 用"开漏输出拉低 + 输入上拉释放"的方式，兼容性最好，
 * 换任意 GPIO 都能跑，速度约 250kHz。
 */
void si2c_init(void);
void si2c_start(void);
void si2c_stop(void);
uint8_t si2c_write(uint8_t byte);   /* 返回 1 表示从机应答(ACK) */

#endif /* I2C_SOFT_H */
