#ifndef RTC_TIME_H
#define RTC_TIME_H

#include <stdint.h>

/*
 * 默认时间：只在 CMake 没有注入编译时刻（RTC_BUILD_*）时才使用的兜底值。
 *
 * 正常流程下不用改这里 —— 每次 cmake configure 会把"编译时刻"通过
 * RTC_BUILD_* 宏注入固件，烧录后首次上电自动把 RTC 设成那个时间。
 * 想改时区改下面的 RTC_TZ_OFFSET_SEC。
 */
#define DEFAULT_YEAR 2026u
#define DEFAULT_MON  10u
#define DEFAULT_DAY  9u
#define DEFAULT_HOUR 12u
#define DEFAULT_MIN  0u
#define DEFAULT_SEC  0u

/* 本地时区相对 UTC 的偏移（秒）。东八区 = +8*3600；改成 0 就是 UTC */
#ifndef RTC_TZ_OFFSET_SEC
#define RTC_TZ_OFFSET_SEC 28800
#endif

/* CMake 注入的编译时刻（UTC），未注入时退回上面的 DEFAULT_* */
#ifndef RTC_BUILD_YEAR
#define RTC_BUILD_YEAR DEFAULT_YEAR
#endif
#ifndef RTC_BUILD_MON
#define RTC_BUILD_MON  DEFAULT_MON
#endif
#ifndef RTC_BUILD_DAY
#define RTC_BUILD_DAY  DEFAULT_DAY
#endif
#ifndef RTC_BUILD_HOUR
#define RTC_BUILD_HOUR DEFAULT_HOUR
#endif
#ifndef RTC_BUILD_MIN
#define RTC_BUILD_MIN  DEFAULT_MIN
#endif
#ifndef RTC_BUILD_SEC
#define RTC_BUILD_SEC  DEFAULT_SEC
#endif

typedef struct {
    uint16_t year;   /* 2000..2099 */
    uint8_t  mon;    /* 1..12 */
    uint8_t  day;    /* 1..31 */
    uint8_t  hour;   /* 0..23 */
    uint8_t  min;    /* 0..59 */
    uint8_t  sec;    /* 0..59 */
    uint8_t  wday;   /* 0=周日 .. 6=周六 */
} datetime_t;

void     rtc_init(void);
void     rtc_get_datetime(datetime_t *dt);
void     rtc_set_datetime(const datetime_t *dt);
uint32_t rtc_get_counter(void);
void     rtc_set_counter(uint32_t counter);
uint8_t  rtc_uses_lse(void);          /* 1 = 外部 32.768kHz 晶振，0 = 内部 LSI（精度差） */

uint32_t datetime_to_counter(const datetime_t *dt);
void     counter_to_datetime(uint32_t counter, datetime_t *dt);
uint8_t  days_in_month(uint16_t year, uint8_t mon);

#endif /* RTC_TIME_H */
