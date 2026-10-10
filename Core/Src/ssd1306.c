#include "ssd1306.h"
#include "i2c_soft.h"
#include "font5x7.h"

/* 显存：按 SSD1306 的页式结构排布，fb[页][列]，可直接整块推给屏幕 */
static uint8_t g_fb[SSD1306_PAGES][SSD1306_WIDTH];

/* 诊断用：I2C 写字节时屏幕没回 ACK 的次数。
   一直涨 = 屏幕其实没收到数据，那画面自然不会变。 */
uint32_t g_i2c_nack = 0u;

static void write_cmd(uint8_t cmd)
{
    si2c_start();
    si2c_write(SSD1306_ADDR);
    si2c_write(0x00u);        /* Co=0, D/C#=0 -> 命令 */
    si2c_write(cmd);
    si2c_stop();
}

static void write_cmd2(uint8_t cmd, uint8_t arg)
{
    si2c_start();
    si2c_write(SSD1306_ADDR);
    si2c_write(0x00u);
    si2c_write(cmd);
    si2c_write(arg);
    si2c_stop();
}

uint8_t ssd1306_init(void)
{
    /* 先探测一下地址有没有应答 */
    si2c_start();
    uint8_t present = si2c_write(SSD1306_ADDR);
    si2c_stop();
    if (!present) {
        return 0u;
    }

    write_cmd(0xAEu);                 /* 关显示 */
    write_cmd2(0x20u, 0x00u);         /* 水平寻址模式 */
    write_cmd(0x21u); write_cmd(0x00u); write_cmd(SSD1306_WIDTH - 1u);   /* 列地址 */
    write_cmd(0x22u); write_cmd(0x00u); write_cmd(SSD1306_PAGES - 1u);   /* 页地址 */
    write_cmd(0x40u);                 /* 起始行 = 0 */
    write_cmd(0xA1u);                 /* 段重映射（左右翻转，常见模块需要） */
    write_cmd(0xC8u);                 /* COM 扫描方向（上下翻转） */
    write_cmd2(0xA8u, SSD1306_HEIGHT - 1u);  /* 复用比 64 */
    write_cmd2(0xD3u, 0x00u);         /* 显示偏移 0 */
    write_cmd2(0xDAu, 0x12u);         /* COM 引脚配置 */
    write_cmd2(0x81u, 0x8Fu);         /* 对比度 */
    write_cmd2(0xD5u, 0x80u);         /* 时钟分频/振荡器 */
    write_cmd2(0xD9u, 0xF1u);         /* 预充电周期 */
    write_cmd2(0xDBu, 0x30u);         /* VCOMH */
    write_cmd2(0x8Du, 0x14u);         /* 使能电荷泵（3.3V 供电必须） */
    write_cmd(0xA4u);                 /* 用显存内容显示 */
    write_cmd(0xA6u);                 /* 正常（不反色） */
    write_cmd(0xAFu);                 /* 开显示 */

    ssd1306_clear();
    ssd1306_refresh();
    return 1u;
}

void ssd1306_refresh(void)
{
    uint16_t i;

    write_cmd(0x21u); write_cmd(0x00u); write_cmd(SSD1306_WIDTH - 1u);
    write_cmd(0x22u); write_cmd(0x00u); write_cmd(SSD1306_PAGES - 1u);

    si2c_start();
    if (!si2c_write(SSD1306_ADDR)) { g_i2c_nack++; }
    if (!si2c_write(0x40u)) { g_i2c_nack++; }   /* Co=0, D/C#=1 -> 数据 */
    for (i = 0u; i < (SSD1306_PAGES * SSD1306_WIDTH); i++) {
        if (!si2c_write(((const uint8_t *)g_fb)[i])) { g_i2c_nack++; }
    }
    si2c_stop();
}

void ssd1306_clear(void)
{
    uint16_t i;
    for (i = 0u; i < (SSD1306_PAGES * SSD1306_WIDTH); i++) {
        ((uint8_t *)g_fb)[i] = 0x00u;
    }
}

void ssd1306_blit(const uint8_t *data)
{
    uint16_t i;

    for (i = 0u; i < (SSD1306_PAGES * SSD1306_WIDTH); i++) {
        ((uint8_t *)g_fb)[i] = data[i];
    }
}

void ssd1306_pixel(int16_t x, int16_t y, uint8_t on)
{
    uint8_t page;
    uint8_t bit;

    if ((x < 0) || (y < 0) || (x >= (int16_t)SSD1306_WIDTH) || (y >= (int16_t)SSD1306_HEIGHT)) {
        return;
    }
    page = (uint8_t)(y / 8u);
    bit  = (uint8_t)(y % 8u);

    if (on) {
        g_fb[page][x] |= (uint8_t)(1u << bit);
    } else {
        g_fb[page][x] &= (uint8_t)~(1u << bit);
    }
}

void ssd1306_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t on)
{
    int16_t i, j;
    for (j = 0; j < h; j++) {
        for (i = 0; i < w; i++) {
            ssd1306_pixel(x + i, y + j, on);
        }
    }
}

void ssd1306_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t on)
{
    int16_t i;
    for (i = 0; i < w; i++) {
        ssd1306_pixel(x + i, y, on);
        ssd1306_pixel(x + i, y + h - 1, on);
    }
    for (i = 0; i < h; i++) {
        ssd1306_pixel(x, y + i, on);
        ssd1306_pixel(x + w - 1, y + i, on);
    }
}

/* data 排布与显存一致：按页打包，每页 w 个字节，bit0 = 页内最上面一行 */
void ssd1306_bitmap(int16_t x, int16_t y, uint16_t w, uint16_t h, const uint8_t *data)
{
    uint16_t page, col;
    uint8_t bit;
    uint16_t pages = h / 8u;

    for (page = 0u; page < pages; page++) {
        for (col = 0u; col < w; col++) {
            uint8_t b = data[page * w + col];
            for (bit = 0u; bit < 8u; bit++) {
                if (b & (uint8_t)(1u << bit)) {
                    ssd1306_pixel(x + (int16_t)col, y + (int16_t)(page * 8u + bit), 1u);
                }
            }
        }
    }
}

void ssd1306_char(int16_t x, int16_t y, char c, uint8_t scale, uint8_t inv)
{
    const uint8_t *g = font5x7_glyph(c);
    uint8_t row, col, sx, sy;

    if (scale == 0u) {
        scale = 1u;
    }

    if (inv) {
        ssd1306_fill_rect(x - 1, y - 1, (int16_t)((FONT_W + 1u) * scale) + 1, (int16_t)(FONT_H * scale) + 2, 1u);
    }

    for (row = 0u; row < FONT_H; row++) {
        uint8_t bits = g[row];
        for (col = 0u; col < FONT_W; col++) {
            if (bits & (uint8_t)(1u << (FONT_W - 1u - col))) {
                for (sy = 0u; sy < scale; sy++) {
                    for (sx = 0u; sx < scale; sx++) {
                        ssd1306_pixel(x + (int16_t)(col * scale + sx),
                                      y + (int16_t)(row * scale + sy),
                                      inv ? 0u : 1u);
                    }
                }
            }
        }
    }
}

uint16_t ssd1306_str_width(const char *s, uint8_t scale)
{
    uint16_t n = 0u;
    if (scale == 0u) {
        scale = 1u;
    }
    while (*s) {
        n += (uint16_t)((FONT_W + 1u) * scale);
        s++;
    }
    return n;
}

void ssd1306_str(int16_t x, int16_t y, const char *s, uint8_t scale, uint8_t inv)
{
    uint16_t step = (uint16_t)((FONT_W + 1u) * ((scale == 0u) ? 1u : scale));

    while (*s) {
        ssd1306_char(x, y, *s, scale, inv);
        x += (int16_t)step;
        s++;
    }
}
