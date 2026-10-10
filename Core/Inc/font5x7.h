#ifndef FONT5X7_H
#define FONT5X7_H

#include <stdint.h>

#define FONT_W 5u
#define FONT_H 7u

/* 返回 7 个字节（行式：每个字节 bit4..bit0 对应一行从左到右的 5 个像素）；
   未收录的字符返回空格的字形。 */
const uint8_t *font5x7_glyph(char c);

#endif /* FONT5X7_H */
