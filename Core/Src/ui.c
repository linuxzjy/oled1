#include "ui.h"
#include "ssd1306.h"
#include "font5x7.h"
#include "rtc_time.h"
#include "cat_bitmap.h"

/* ------------------------------ 界面状态 ------------------------------ */
typedef enum { PAGE_HOME = 0, PAGE_CLOCK, PAGE_COUNT } page_t;
typedef enum { F_HOUR = 0, F_MIN, F_SEC, F_YEAR, F_MON, F_DAY, F_COUNT } field_t;

static page_t     g_page = PAGE_HOME;
static int8_t     g_field = -1;          /* -1 = 非设置模式，否则为当前编辑字段 */
static datetime_t g_edit;
static uint8_t    g_dirty = 1u;

static const char *const g_wday[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };

/* 小猫：每 5 秒眨一次眼（持续 200ms） */
#define BLINK_PERIOD_MS 5000u
#define BLINK_TIME_MS   200u

static uint8_t cat_frame(uint32_t ms)
{
    return ((ms % BLINK_PERIOD_MS) >= (BLINK_PERIOD_MS - BLINK_TIME_MS)) ? 1u : 0u;
}

/* ------------------------------ 小工具 ------------------------------ */
static uint8_t wrap_add(uint8_t v, int32_t d, uint8_t mod)
{
    int32_t t = (int32_t)v + d;

    t %= (int32_t)mod;
    if (t < 0) {
        t += (int32_t)mod;
    }
    return (uint8_t)t;
}

static void clamp_day(void)
{
    uint8_t max = days_in_month(g_edit.year, g_edit.mon);
    if (g_edit.day > max) {
        g_edit.day = max;
    }
}

static void fmt2(char *out, uint8_t v)
{
    out[0] = (char)('0' + (v / 10u));
    out[1] = (char)('0' + (v % 10u));
    out[2] = '\0';
}

static void fmt_date(char *out, uint16_t year, uint8_t mon, uint8_t day)
{
    out[0]  = (char)('0' + (year / 1000u));
    out[1]  = (char)('0' + ((year / 100u) % 10u));
    out[2]  = (char)('0' + ((year / 10u) % 10u));
    out[3]  = (char)('0' + (year % 10u));
    out[4]  = '-';
    out[5]  = (char)('0' + (mon / 10u));
    out[6]  = (char)('0' + (mon % 10u));
    out[7]  = '-';
    out[8]  = (char)('0' + (day / 10u));
    out[9]  = (char)('0' + (day % 10u));
    out[10] = '\0';
}

static void draw_2digits(int16_t x, int16_t y, uint8_t v, uint8_t scale, uint8_t inv)
{
    char b[3];

    fmt2(b, v);
    ssd1306_char(x, y, b[0], scale, inv);
    ssd1306_char((int16_t)(x + (int16_t)((FONT_W + 1u) * scale)), y, b[1], scale, inv);
}

/* ------------------------------ 对外接口 ------------------------------ */
void ui_init(void)
{
    g_page = PAGE_HOME;
    g_field = -1;
    g_dirty = 1u;
}

void ui_encoder(int32_t delta)
{
    if (delta == 0) {
        return;
    }
    g_dirty = 1u;

    if (g_field < 0) {
        int32_t p = ((int32_t)g_page + delta) % (int32_t)PAGE_COUNT;
        if (p < 0) {
            p += (int32_t)PAGE_COUNT;
        }
        g_page = (page_t)p;
        return;
    }

    switch (g_field) {
    case F_HOUR:
        g_edit.hour = wrap_add(g_edit.hour, delta, 24u);
        break;
    case F_MIN:
        g_edit.min = wrap_add(g_edit.min, delta, 60u);
        break;
    case F_SEC:
        g_edit.sec = wrap_add(g_edit.sec, delta, 60u);
        break;
    case F_YEAR:
        g_edit.year = (uint16_t)(2000u + wrap_add((uint8_t)(g_edit.year - 2000u), delta, 100u));
        clamp_day();
        break;
    case F_MON:
        g_edit.mon = (uint8_t)(wrap_add((uint8_t)(g_edit.mon - 1u), delta, 12u) + 1u);
        clamp_day();
        break;
    case F_DAY:
        g_edit.day = (uint8_t)(wrap_add((uint8_t)(g_edit.day - 1u), delta,
                                        days_in_month(g_edit.year, g_edit.mon)) + 1u);
        break;
    default:
        break;
    }
}

void ui_key(key_id_t id)
{
    g_dirty = 1u;

    if (g_field < 0) {
        if ((id == KEY_CON) || (id == KEY_PSH)) {
            rtc_get_datetime(&g_edit);
            g_field = (int8_t)F_HOUR;
        } else if (id == KEY_BACK) {
            g_page = PAGE_HOME;
        }
        return;
    }

    if ((id == KEY_CON) || (id == KEY_PSH)) {
        g_field++;
        if (g_field >= (int8_t)F_COUNT) {
            rtc_set_datetime(&g_edit);      /* 最后一项之后再按 = 保存 */
            g_field = -1;
        }
    } else if (id == KEY_BACK) {
        g_field = -1;                       /* 放弃修改 */
    }
}

uint8_t ui_need_redraw(const datetime_t *dt, uint32_t ms)
{
    static datetime_t last_dt;
    static uint8_t    last_frame = 0xFFu;
    static uint8_t    first = 1u;
    uint8_t need;

    if (first) {
        first = 0u;
        last_dt = *dt;
        last_frame = cat_frame(ms);
        return 1u;
    }

    need = g_dirty;
    if ((dt->sec != last_dt.sec) || (dt->min != last_dt.min) || (dt->hour != last_dt.hour) ||
        (dt->day != last_dt.day) || (dt->mon != last_dt.mon) || (dt->year != last_dt.year)) {
        need = 1u;
    }
    if (cat_frame(ms) != last_frame) {
        need = 1u;
    }

    last_dt = *dt;
    last_frame = cat_frame(ms);

    return need;
}

/* ------------------------------ 三个页面 ------------------------------ */
static void draw_home(const datetime_t *dt, uint32_t ms)
{
    char buf[16];

    /* 左：小猫 */
    ssd1306_bitmap(0, 4, CAT_IMG_W, CAT_IMG_H,
                   cat_frame(ms) ? cat_bitmap_blink : cat_bitmap_open);

    /* 右：日期 / 时间 / 星期 / 秒 */
    fmt_date(buf, dt->year, dt->mon, dt->day);
    ssd1306_str(58, 4, buf, 1, 0);

    draw_2digits(58, 18, dt->hour, 2, 0);
    ssd1306_char(58 + 24, 18, ':', 2, 0);
    draw_2digits(58 + 36, 18, dt->min, 2, 0);

    ssd1306_str(58, 38, g_wday[dt->wday % 7u], 1, 0);
    ssd1306_char(84, 38, ':', 1, 0);
    draw_2digits(90, 38, dt->sec, 1, 0);

    ssd1306_str(58, 52, "CON SET", 1, 0);
}

static void draw_clock(const datetime_t *dt)
{
    char buf[16];

    draw_2digits(16, 12, dt->hour, 2, 0);
    ssd1306_char(16 + 24, 12, ':', 2, 0);
    draw_2digits(16 + 36, 12, dt->min, 2, 0);
    ssd1306_char(16 + 60, 12, ':', 2, 0);
    draw_2digits(16 + 72, 12, dt->sec, 2, 0);

    fmt_date(buf, dt->year, dt->mon, dt->day);
    ssd1306_str(34, 36, buf, 1, 0);
    ssd1306_str(55, 48, g_wday[dt->wday % 7u], 1, 0);
    ssd1306_str(13, 57, "ENC PAGE  CON SET", 1, 0);
}

static void draw_setting(void)
{
    uint8_t  i;
    int16_t  x;
    char     date[16];
    uint8_t  step2 = (uint8_t)((FONT_W + 1u) * 2u);

    ssd1306_str(40, 2, "SET TIME", 1, 0);

    /* 时:分:秒，当前字段反白 */
    x = 16;
    draw_2digits(x, 14, g_edit.hour, 2, (g_field == F_HOUR));
    x += 2 * (int16_t)step2;
    ssd1306_char(x, 14, ':', 2, 0);
    x += (int16_t)step2;
    draw_2digits(x, 14, g_edit.min, 2, (g_field == F_MIN));
    x += 2 * (int16_t)step2;
    ssd1306_char(x, 14, ':', 2, 0);
    x += (int16_t)step2;
    draw_2digits(x, 14, g_edit.sec, 2, (g_field == F_SEC));

    /* 年-月-日：按字符下标判断属于哪个字段 */
    fmt_date(date, g_edit.year, g_edit.mon, g_edit.day);
    x = 34;
    for (i = 0u; i < 10u; i++) {
        uint8_t inv = 0u;

        if ((i <= 3u) && (g_field == (int8_t)F_YEAR)) {
            inv = 1u;
        } else if (((i == 5u) || (i == 6u)) && (g_field == (int8_t)F_MON)) {
            inv = 1u;
        } else if (((i == 8u) || (i == 9u)) && (g_field == (int8_t)F_DAY)) {
            inv = 1u;
        }
        ssd1306_char(x, 38, date[i], 1, inv);
        x += (int16_t)(FONT_W + 1u);
    }

    ssd1306_str(7, 52, "CON NEXT  BACK EXIT", 1, 0);
}

void ui_draw(const datetime_t *dt, uint32_t ms)
{
    ssd1306_clear();

    if (g_field >= 0) {
        draw_setting();
    } else if (g_page == PAGE_HOME) {
        draw_home(dt, ms);
    } else {
        draw_clock(dt);
    }

    g_dirty = 0u;
}
