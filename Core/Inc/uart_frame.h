#ifndef UART_FRAME_H
#define UART_FRAME_H

#include <stdint.h>

/*
 * 上位机 -> MCU 的串口帧协议（用于把 PC 处理好的图像推到 OLED 上）
 *
 *   [0]      0xAA
 *   [1]      0x55
 *   [2..3]   payload 长度，小端（本工程固定 1024）
 *   [4]      命令字：0x01 = 128x64 1bpp 位图
 *   [5..]    payload（1024 字节）
 *   [最后]   校验：从 [2] 起到最后一个 payload 字节的累加和 & 0xFF
 *
 * 位图格式与 SSD1306 显存完全一致，收到后可直接 memcpy 进显存：
 *   共 8 页 x 128 列；页 0 对应 y=0..7，页 1 对应 y=8..15，...
 *   每字节 bit0 = 该页最上面一行，bit7 = 最下面一行
 *   列顺序 x = 0..127
 *
 * Python 端这样打包（numpy）：
 *   packed = np.packbits(bw, axis=0, bitorder='little')   # (64,128) -> (8,128)
 *   frame  = packed.tobytes()                              # 1024 字节
 */

#define FRAME_PAYLOAD_MAX 1024u

/* 初始化 USART + DMA 接收（循环 DMA + IDLE 中断，收帧几乎不占 CPU） */
void    uart_frame_init(void);

/* 主循环调用：取到一帧就拷进 out（需 1024 字节）并返回 1，没有则返回 0 */
uint8_t uart_frame_take(uint8_t *out);

/* 向 PC 回发数据（调试/ACK 用，阻塞式） */
void    uart_frame_send(const uint8_t *data, uint16_t len);

/* 统计：收到的正确帧数 / 校验失败的帧数，方便排查链路 */
void    uart_frame_stats(uint32_t *ok, uint32_t *bad);

#endif /* UART_FRAME_H */
