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

#endif /* UI_H */
