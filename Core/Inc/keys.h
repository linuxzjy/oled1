#ifndef KEYS_H
#define KEYS_H

#include <stdint.h>

typedef enum {
    KEY_NONE = 0,
    KEY_CON,      /* CON 确认键 */
    KEY_BACK,     /* BACK 返回键 */
    KEY_PSH       /* EC11 自带按键 PSH */
} key_id_t;

void    keys_init(void);
void    keys_scan_1ms(void);        /* 在 SysTick(1ms) 里调用 */
key_id_t keys_take_event(void);     /* 取走一次按下事件，无事件返回 KEY_NONE */

#endif /* KEYS_H */
