#include "rtc_time.h"
#include "delay.h"
#include "stm32f1xx.h"

/*
 * 用 STM32F103 内置 RTC 做实时时钟。
 * 时钟源优先 LSE(32.768kHz，PC14/PC15 上的晶振，精度高)；
 * 起不来就退到内部 LSI(约 40kHz，误差较大，仅供没有晶振的板子兜底)。
 * RTC 秒计数器含义：自 2000-01-01 00:00:00 起的秒数。
 */

#define BKP_MAGIC       0xA55Au
#define DAYS_1970_2000  10957u          /* 1970-01-01 到 2000-01-01 的天数 */

/* 32.768kHz 晶振起振慢，给足 2.5 秒（原来用空循环约 0.4 秒，会误判成没晶振） */
#define LSE_TIMEOUT_MS  2500u

static uint8_t  g_lse = 0u;
static uint32_t g_prescaler = 32768UL - 1UL;

static void rtc_wait_ready(void)
{
    uint32_t t = 0u;
    while (!(RTC->CRL & RTC_CRL_RTOFF)) {
        if (++t > 2000000UL) { break; }
    }
}

static void rtc_enter_config(void)
{
    rtc_wait_ready();
    RTC->CRL |= RTC_CRL_CNF;           /* 进入配置模式，此后才能写 PRL/CNT */
}

static void rtc_exit_config(void)
{
    RTC->CRL &= ~RTC_CRL_CNF;
    rtc_wait_ready();
}

static int days_from_civil(int y, unsigned m, unsigned d)
{
    int era;
    unsigned yoe, doy, doe;

    y -= (m <= 2u) ? 1 : 0;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (unsigned)(y - era * 400);
    doy = (153u * (m + (m > 2u ? (unsigned)(-3) : 9u)) + 2u) / 5u + d - 1u;
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int)doe - 719468;
}

static void civil_from_days(int z, int *y, unsigned *m, unsigned *d)
{
    int era;
    unsigned doe, yoe, doy, mp;
    int yy;

    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned)(z - era * 146097);
    yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    yy = (int)yoe + era * 400;
    doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    mp = (5u * doy + 2u) / 153u;
    *d = doy - (153u * mp + 2u) / 5u + 1u;
    *m = mp + (mp < 10u ? 3u : (unsigned)(-9));
    *y = yy + ((*m <= 2u) ? 1 : 0);
}

uint8_t days_in_month(uint16_t year, uint8_t mon)
{
    static const uint8_t tbl[12] = { 31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u };
    uint8_t days;

    if ((mon < 1u) || (mon > 12u)) {
        return 30u;
    }
    days = tbl[mon - 1u];
    if ((mon == 2u) && (((year % 4u == 0u) && (year % 100u != 0u)) || (year % 400u == 0u))) {
        days = 29u;
    }
    return days;
}

uint32_t datetime_to_counter(const datetime_t *dt)
{
    int days = days_from_civil((int)dt->year, dt->mon, dt->day);
    uint32_t d2000 = (uint32_t)(days - (int)DAYS_1970_2000);

    return d2000 * 86400UL + (uint32_t)dt->hour * 3600UL
           + (uint32_t)dt->min * 60UL + (uint32_t)dt->sec;
}

void counter_to_datetime(uint32_t counter, datetime_t *dt)
{
    uint32_t days = counter / 86400UL;
    uint32_t rem = counter % 86400UL;
    int y;
    unsigned m, d;

    civil_from_days((int)(days + DAYS_1970_2000), &y, &m, &d);

    dt->year = (uint16_t)y;
    dt->mon = (uint8_t)m;
    dt->day = (uint8_t)d;
    dt->hour = (uint8_t)(rem / 3600UL);
    dt->min = (uint8_t)((rem % 3600UL) / 60UL);
    dt->sec = (uint8_t)(rem % 60UL);
    dt->wday = (uint8_t)((days + 6u) % 7u);     /* 2000-01-01 是周六 */
}

uint32_t rtc_get_counter(void)
{
    uint32_t a, b;

    do {
        a = ((uint32_t)RTC->CNTH << 16) | (uint32_t)RTC->CNTL;
        b = ((uint32_t)RTC->CNTH << 16) | (uint32_t)RTC->CNTL;
    } while (a != b);

    return a;
}

void rtc_set_counter(uint32_t counter)
{
    rtc_enter_config();
    RTC->CNTH = (uint16_t)(counter >> 16);
    RTC->CNTL = (uint16_t)(counter & 0xFFFFu);
    rtc_exit_config();
}

void rtc_get_datetime(datetime_t *dt)
{
    counter_to_datetime(rtc_get_counter(), dt);
}

void rtc_set_datetime(const datetime_t *dt)
{
    rtc_set_counter(datetime_to_counter(dt));
}

uint8_t rtc_uses_lse(void)
{
    return g_lse;
}

/* 起 LSE 晶振，按毫秒等待；成功返回 1。用 delay_ms 而不是空循环，
   否则换主频/换优化等级就会让超时时间漂移，把好晶振误判成没有晶振。 */
static uint8_t rtc_start_lse(uint16_t timeout_ms)
{
    uint16_t i;

    if (RCC->BDCR & RCC_BDCR_LSERDY) {
        return 1u;
    }

    RCC->BDCR |= RCC_BDCR_LSEON;
    for (i = 0u; i < timeout_ms; i++) {
        delay_ms(1u);
        if (RCC->BDCR & RCC_BDCR_LSERDY) {
            return 1u;
        }
    }

    RCC->BDCR &= ~RCC_BDCR_LSEON;             /* 确实没晶振，关掉省电 */
    return 0u;
}

static void rtc_start_lsi(void)
{
    RCC->CSR |= RCC_CSR_LSION;
    while (!(RCC->CSR & RCC_CSR_LSIRDY)) { }
}

/* 备份域还在（VBAT 没掉电）时也要保证时钟源在跑：
   系统复位会清掉 RCC_CSR 的 LSION，不重新打开 RTC 就直接停走。 */
static void rtc_ensure_clock(void)
{
    if ((RCC->BDCR & RCC_BDCR_RTCSEL) == RCC_BDCR_RTCSEL_LSE) {
        if (!rtc_start_lse(LSE_TIMEOUT_MS)) {
            /* 晶振中途掉线，兜底切 LSI（分频稍后统一重写） */
            rtc_start_lsi();
            RCC->BDCR &= ~RCC_BDCR_RTCSEL;
            RCC->BDCR |= RCC_BDCR_RTCSEL_LSI;
        }
    } else {
        rtc_start_lsi();
    }
}

/* 按当前 RTCSEL 写预分频：LSE 32768-1，LSI 40000-1 */
static void rtc_apply_prescaler(void)
{
    uint32_t prl;

    if ((RCC->BDCR & RCC_BDCR_RTCSEL) == RCC_BDCR_RTCSEL_LSE) {
        g_lse = 1u;
        prl = 32768UL - 1UL;
    } else {
        g_lse = 0u;
        prl = 40000UL - 1UL;
    }
    g_prescaler = prl;

    rtc_enter_config();
    RTC->PRLH = (uint16_t)(prl >> 16);
    RTC->PRLL = (uint16_t)(prl & 0xFFFFu);
    rtc_exit_config();
}

/* 编译时刻对应的秒计数（CMake 注入 RTC_BUILD_*，再按时区偏移成本地时间） */
static uint32_t rtc_build_stamp(void)
{
    datetime_t dt;

    dt.year = (uint16_t)RTC_BUILD_YEAR;
    dt.mon  = (uint8_t)RTC_BUILD_MON;
    dt.day  = (uint8_t)RTC_BUILD_DAY;
    dt.hour = (uint8_t)RTC_BUILD_HOUR;
    dt.min  = (uint8_t)RTC_BUILD_MIN;
    dt.sec  = (uint8_t)RTC_BUILD_SEC;
    dt.wday = 0u;

    return datetime_to_counter(&dt) + (uint32_t)RTC_TZ_OFFSET_SEC;
}

void rtc_init(void)
{
    uint32_t stamp;
    uint32_t stored;
    uint16_t i;

    /* 访问备份域必须先开 PWR/BKP 时钟并置 DBP */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN | RCC_APB1ENR_BKPEN;
    PWR->CR |= PWR_CR_DBP;

    stamp = rtc_build_stamp();

    if (BKP->DR1 != BKP_MAGIC) {
        /* 首次上电 / 后备电池掉过电：重新选时钟源 */
        RCC->BDCR |= RCC_BDCR_BDRST;          /* 复位备份域 */
        RCC->BDCR &= ~RCC_BDCR_BDRST;

        if (rtc_start_lse(LSE_TIMEOUT_MS)) {
            RCC->BDCR &= ~RCC_BDCR_RTCSEL;
            RCC->BDCR |= RCC_BDCR_RTCSEL_LSE;
        } else {
            /* 没有 32.768kHz 晶振，退到 LSI */
            rtc_start_lsi();
            RCC->BDCR &= ~RCC_BDCR_RTCSEL;
            RCC->BDCR |= RCC_BDCR_RTCSEL_LSI;
        }
        RCC->BDCR |= RCC_BDCR_RTCEN;
    } else {
        rtc_ensure_clock();
    }

    rtc_apply_prescaler();

    /* 烧了新固件就把时间刷新为编译时刻。
       DR2/DR3 存上次写入的秒计数做指纹，一致就说明没换固件，不要覆盖已走的时间。 */
    stored = ((uint32_t)BKP->DR3 << 16) | (uint32_t)BKP->DR2;
    if (stored != stamp) {
        rtc_set_counter(stamp);
        BKP->DR2 = (uint16_t)(stamp & 0xFFFFu);
        BKP->DR3 = (uint16_t)(stamp >> 16);
    }

    BKP->DR1 = BKP_MAGIC;                     /* 下次上电不再重选时钟源 */

    /* 等一次同步，保证后面读 CNT 有效 */
    RTC->CRL &= ~RTC_CRL_RSF;
    for (i = 0u; i < 2000u; i++) {
        delay_ms(1u);
        if (RTC->CRL & RTC_CRL_RSF) { break; }
    }
}
