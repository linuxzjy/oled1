#include "ec11.h"
#include "pins.h"
#include "stm32f1xx.h"

/* 旋转方向反了就把这里改成 1 */
#define EC11_REVERSE 0

/* 20 定位/圈，AB 相 4 倍频 -> 每定位 4 个计数 */
#define COUNTS_PER_DETENT 4

static uint16_t g_prev_cnt = 0u;
static int32_t  g_acc = 0;     /* 未凑满一个定位的残余计数 */

void ec11_init(void)
{
    if (EC11_PORT == GPIOA)      { RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; }
    else if (EC11_PORT == GPIOB) { RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; }
    else if (EC11_PORT == GPIOC) { RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; }
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* PA0 / PA1：输入上拉（EC11 的公共脚接 GND，A/B 需要上拉） */
    EC11_PORT->CRL &= ~((0xFU << (EC11_A_PIN * 4u)) | (0xFU << (EC11_B_PIN * 4u)));
    EC11_PORT->CRL |= ((0x8UL << (EC11_A_PIN * 4u)) | (0x8UL << (EC11_B_PIN * 4u)));
    EC11_PORT->ODR |= ((1UL << EC11_A_PIN) | (1UL << EC11_B_PIN));

    /* TIM2 编码器接口 */
    EC11_TIM->CR1 = 0u;
    EC11_TIM->PSC = 0u;
    EC11_TIM->ARR = 0xFFFFu;
    EC11_TIM->CNT = 0u;

    /* tDTS = 4*tCK_INT，配合最大输入滤波，抑制触点抖动 */
    EC11_TIM->CR1 |= (0x2UL << 8);                       /* CKD = 10 */
    EC11_TIM->CCMR1 = (TIM_CCMR1_CC1S_0 | TIM_CCMR1_CC2S_0)
                    | (0xFUL << 4)                       /* IC1F = 1111 */
                    | (0xFUL << 12);                     /* IC2F = 1111 */
    EC11_TIM->CCER &= ~(TIM_CCER_CC1P | TIM_CCER_CC2P);  /* 上升沿/不反相 */
    EC11_TIM->SMCR = (EC11_TIM->SMCR & ~TIM_SMCR_SMS) | 0x3UL;  /* SMS=011：TI1/TI2 双边沿 4 倍频 */
    EC11_TIM->CR1 |= TIM_CR1_CEN;

    g_prev_cnt = (uint16_t)EC11_TIM->CNT;
    g_acc = 0;
}

int32_t ec11_take_delta(void)
{
    uint16_t now = (uint16_t)EC11_TIM->CNT;
    int16_t  diff = (int16_t)(now - g_prev_cnt);
    int32_t  detents;

    g_prev_cnt = now;
    g_acc += (int32_t)diff;

    detents = g_acc / COUNTS_PER_DETENT;
    g_acc -= detents * COUNTS_PER_DETENT;

#if EC11_REVERSE
    detents = -detents;
#endif

    return detents;
}
