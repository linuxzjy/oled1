#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define SSD1306_WIDTH  128u
#define SSD1306_HEIGHT 64u
#define SSD1306_PAGES  (SSD1306_HEIGHT / 8u)

/* 7 位地址 0x3C（部分模块是 0x3D，改这里即可） */
#define SSD1306_ADDR7  0x3Cu
#define SSD1306_ADDR   (SSD1306_ADDR7 << 1u)

/* 返回 1 = 在 I2C 上找到了屏幕 */
uint8_t ssd1306_init(void);
void    ssd1306_refresh(void);              /* 把显存整体推到屏幕 */
void    ssd1306_clear(void);

void    ssd1306_pixel(int16_t x, int16_t y, uint8_t on);
void    ssd1306_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t on);
void    ssd1306_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t on);
void    ssd1306_bitmap(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *data);

/* scale = 放大倍数(1/2/3...)；inv = 1 时反白显示 */
void    ssd1306_char(int16_t x, int16_t y, char c, uint8_t scale, uint8_t inv);
void    ssd1306_str(int16_t x, int16_t y, const char *s, uint8_t scale, uint8_t inv);
uint16_t ssd1306_str_width(const char *s, uint8_t scale);

#endif /* SSD1306_H */
