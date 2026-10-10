#ifndef UI_H
#define UI_H

#include <stdint.h>
#include "rtc_time.h"
#include "keys.h"

void    ui_init(void);
void    ui_encoder(int32_t delta);                 /* EC11 转动了多少个定位 */
void    ui_key(key_id_t id);                       /* 按键事件 */
void    ui_draw(const datetime_t *dt, uint32_t ms);/* 画一帧（不刷新到屏） */
uint8_t ui_need_redraw(const datetime_t *dt, uint32_t ms);

/* 喂进一帧 128x64 1bpp 位图（1024 字节，格式见 uart_frame.h）。
   只有第一次收到画面会自动跳到相机页；之后按 BACK 回主页、转 EC11 翻页，
   都不会被下一帧强行拉回。 */
void    ui_feed_bitmap(const uint8_t *data);

/* 当前是否正停在相机页（串口来帧时决定要不要立刻刷屏） */
uint8_t ui_is_cam_page(void);

#endif /* UI_H */
