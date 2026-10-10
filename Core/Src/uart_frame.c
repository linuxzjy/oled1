#include "uart_frame.h"
#include "pins.h"
#include "stm32f1xx.h"

/*
 * USART1(PA9/PA10) + DMA 循环接收 + IDLE 中断。
 * IDLE 表示"线路空闲了一帧的时间"，也就是 PC 这一帧发完了，
 * 这样每帧只进一次中断，921600bps 下 CPU 占用也极低。
 */

#define SYNC1       0xAAu
#define SYNC2       0x55u
#define CMD_BITMAP  0x01u
#define DMA_SIZE    2048u

enum { S_SYNC1 = 0u, S_SYNC2, S_LEN_L, S_LEN_H, S_CMD, S_DATA, S_SUM };

static uint8_t  g_dma[DMA_SIZE];
static uint16_t g_dma_pos;                  /* 已解析到的 DMA 位置 */

/* 乒乓双缓冲：中断写其中一个，主循环读另一个，不会读到半帧 */
static uint8_t  g_buf[2][FRAME_PAYLOAD_MAX];
static uint8_t  g_writing;
static volatile uint8_t g_newest;
static volatile uint8_t g_ready;

static uint8_t  g_state;
static uint16_t g_len;
static uint8_t  g_cmd;
static uint16_t g_idx;
static uint8_t  g_sum;

static volatile uint32_t g_ok;
static volatile uint32_t g_bad;

static void frame_feed(uint8_t b)
{
    switch (g_state) {
    case S_SYNC1:
        g_state = (b == SYNC1) ? (uint8_t)S_SYNC2 : (uint8_t)S_SYNC1;
        break;

    case S_SYNC2:
        g_state = (b == SYNC2) ? (uint8_t)S_LEN_L : (uint8_t)S_SYNC1;
        break;

    case S_LEN_L:
        g_len = b;
        g_sum = b;
        g_state = (uint8_t)S_LEN_H;
        break;

    case S_LEN_H:
        g_len |= (uint16_t)((uint16_t)b << 8);
        g_sum = (uint8_t)(g_sum + b);
        /* 只支持固定 1024 字节的位图帧，其它长度直接丢弃重新同步 */
        g_state = (g_len == FRAME_PAYLOAD_MAX) ? (uint8_t)S_CMD : (uint8_t)S_SYNC1;
        break;

    case S_CMD:
        g_cmd = b;
        g_sum = (uint8_t)(g_sum + b);
        g_idx = 0u;
        g_state = (b == CMD_BITMAP) ? (uint8_t)S_DATA : (uint8_t)S_SYNC1;
        break;

    case S_DATA:
        g_buf[g_writing][g_idx] = b;
        g_sum = (uint8_t)(g_sum + b);
        g_idx++;
        if (g_idx >= g_len) {
            g_state = (uint8_t)S_SUM;
        }
        break;

    case S_SUM:
        if (g_sum == b) {
            g_newest = g_writing;
            g_writing = (uint8_t)(g_writing ^ 1u);
            g_ready = 1u;
            g_ok++;
        } else {
            g_bad++;
        }
        g_state = (uint8_t)S_SYNC1;
        break;

    default:
        g_state = (uint8_t)S_SYNC1;
        break;
    }
}

void uart_frame_init(void)
{
    uint32_t ck;

    /* --- 时钟 --- */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN;
    RCC->AHBENR  |= HOST_UART_DMAEN;

    /* --- PA9 TX：复用推挽 50MHz(1011b)；PA10 RX：浮空输入(0100b) --- */
    HOST_UART_PORT->CRH &= ~((0xFu << 4) | (0xFu << 8));
    HOST_UART_PORT->CRH |=  ((0xBu << 4) | (0x4u << 8));

    /* --- 波特率 ---
     * BRR 里放的是 16*USARTDIV（高 12 位整数 + 低 4 位小数），
     * 而 USARTDIV = fCK / (16 * baud)，两者一消就得到：BRR = fCK / baud。
     * 千万别先算 fCK/baud 再 <<4，那会整整差 16 倍。
     * 921600 时 BRR = 72000000/921600 = 78 = 0x4E（即 DIV=4.875，实际 923076，误差 0.16%） */
    ck = SystemCoreClock;                       /* USART1 挂 APB2，本工程预分频固定为 1 */
    HOST_UART->BRR = (uint16_t)((ck + (HOST_UART_BAUD / 2u)) / HOST_UART_BAUD);

    /* --- DMA1_Channel5：循环模式收 USART1->DR --- */
    HOST_UART_DMA->CCR   = 0u;
    HOST_UART_DMA->CPAR  = (uint32_t)&HOST_UART->DR;
    HOST_UART_DMA->CMAR  = (uint32_t)g_dma;
    HOST_UART_DMA->CNDTR = (uint16_t)DMA_SIZE;
    HOST_UART_DMA->CCR   = DMA_CCR_MINC | DMA_CCR_CIRC | DMA_CCR_PL_1;

    HOST_UART->CR3 |= USART_CR3_DMAR;           /* 接收走 DMA */
    HOST_UART->CR1  = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | USART_CR1_IDLEIE;

    HOST_UART_DMA->CCR |= DMA_CCR_EN;

    NVIC_EnableIRQ(HOST_UART_IRQn);
}

void USART1_IRQHandler(void)
{
    uint32_t sr = USART1->SR;

    if (sr & USART_SR_IDLE) {
        uint32_t tmp;
        uint16_t pos;

        tmp = USART1->SR;                       /* 清 IDLE：先读 SR 再读 DR */
        tmp = USART1->DR;
        (void)tmp;

        pos = (uint16_t)(DMA_SIZE - (uint16_t)HOST_UART_DMA->CNDTR);
        if (pos >= DMA_SIZE) {
            pos = 0u;
        }

        while (g_dma_pos != pos) {              /* 把新收到的字节喂给状态机 */
            frame_feed(g_dma[g_dma_pos]);
            g_dma_pos++;
            if (g_dma_pos >= DMA_SIZE) {
                g_dma_pos = 0u;
            }
        }
    } else if (sr & (USART_SR_ORE | USART_SR_FE | USART_SR_NE)) {
        uint32_t tmp = USART1->DR;              /* 出错也要读 DR，否则中断出不去 */
        (void)tmp;
    }
}

uint8_t uart_frame_take(uint8_t *out)
{
    uint8_t  n;
    uint16_t i;

    if (g_ready == 0u) {
        return 0u;
    }

    n = g_newest;
    g_ready = 0u;
    for (i = 0u; i < FRAME_PAYLOAD_MAX; i++) {
        out[i] = g_buf[n][i];
    }
    return 1u;
}

void uart_frame_send(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0u; i < len; i++) {
        while (!(USART1->SR & USART_SR_TXE)) { }
        USART1->DR = data[i];
    }
    while (!(USART1->SR & USART_SR_TC)) { }
}

void uart_frame_stats(uint32_t *ok, uint32_t *bad)
{
    *ok  = g_ok;
    *bad = g_bad;
}
