#ifndef EC11_H
#define EC11_H

#include <stdint.h>

/*
 * EC11：20 定位/圈（转一圈 20 个"咔哒"）。AB 相接在 TIM2_CH1/CH2(PA0/PA1)，
 * 用定时器的编码器接口做 4 倍频，每个定位 = 4 个计数。
 * 若旋转方向和预期相反，把 ec11.c 里的 EC11_REVERSE 改成 1。
 */
void    ec11_init(void);
int32_t ec11_take_delta(void);    /* 返回自上次取走以来的定位数；正 = 顺时针 */

#endif /* EC11_H */
