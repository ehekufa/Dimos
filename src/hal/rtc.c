#include "../include/hal.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

#define CMOS_SECONDS  0x00
#define CMOS_MINUTES  0x02
#define CMOS_HOURS    0x04
#define CMOS_DAY      0x07
#define CMOS_MONTH    0x08
#define CMOS_YEAR     0x09
#define CMOS_CENTURY  0x32
#define CMOS_STATUS_A 0x0A
#define CMOS_STATUS_B 0x0B

#define CMOS_BINARY_MODE       0x04
#define CMOS_TWENTY_FOUR_HOURS 0x02
#define CMOS_HOUR_IS_PM        0x80

static uint8_t  g_boot_seconds = 0;
static uint8_t  g_boot_minutes = 0;
static uint8_t  g_boot_hours   = 12;
static uint8_t  g_boot_day     = 1;
static uint8_t  g_boot_month   = 1;
static uint16_t g_boot_year    = 2026;
static uint64_t g_seconds_at_boot = 12 * 3600;

static uint8_t cmos_read(uint8_t reg) {
    HalOutPort8(CMOS_ADDR, reg);
    return HalInPort8(CMOS_DATA);
}

static uint8_t cmos_bcd_or_bin(uint8_t raw, bool binary) {
    if (binary) return raw;
    return (uint8_t)(((raw >> 4) * 10) + (raw & 0x0F));
}

void HalClockInit(void) {
    /* Wait briefly if RTC update-in-progress bit (0x80) is set */
    for (uint32_t i = 0; i < 10000; i++) {
        if ((cmos_read(CMOS_STATUS_A) & 0x80) == 0) break;
    }

    uint8_t status_b = cmos_read(CMOS_STATUS_B);
    bool binary = (status_b & CMOS_BINARY_MODE) != 0;

    uint8_t raw_hours = cmos_read(CMOS_HOURS);
    uint8_t hours = cmos_bcd_or_bin((uint8_t)(raw_hours & 0x7F), binary);
    if ((status_b & CMOS_TWENTY_FOUR_HOURS) == 0 && (raw_hours & CMOS_HOUR_IS_PM) != 0) {
        if (hours < 12) hours = (uint8_t)(hours + 12);
    } else if ((status_b & CMOS_TWENTY_FOUR_HOURS) == 0 && hours == 12) {
        hours = 0;
    }

    g_boot_seconds = cmos_bcd_or_bin(cmos_read(CMOS_SECONDS), binary);
    g_boot_minutes = cmos_bcd_or_bin(cmos_read(CMOS_MINUTES), binary);
    g_boot_hours   = hours;
    g_boot_day     = cmos_bcd_or_bin(cmos_read(CMOS_DAY), binary);
    g_boot_month   = cmos_bcd_or_bin(cmos_read(CMOS_MONTH), binary);

    uint8_t century = cmos_read(CMOS_CENTURY);
    if (century >= 0x19 && century <= 0x21) {
        century = (uint8_t)(((century >> 4) * 10) + (century & 0x0F));
    } else {
        century = 20;
    }
    g_boot_year = (uint16_t)(century * 100 + cmos_bcd_or_bin(cmos_read(CMOS_YEAR), binary));

    if (g_boot_hours > 23 || g_boot_minutes > 59 || g_boot_seconds > 59 ||
        g_boot_day < 1 || g_boot_day > 31 || g_boot_month < 1 || g_boot_month > 12) {
        g_boot_hours   = 12;
        g_boot_minutes = 0;
        g_boot_seconds = 0;
        g_boot_day     = 1;
        g_boot_month   = 1;
        g_boot_year    = 2026;
    }

    g_seconds_at_boot = (uint64_t)g_boot_hours * 3600ULL +
                        (uint64_t)g_boot_minutes * 60ULL +
                        (uint64_t)g_boot_seconds;

    HalPrintf("[HAL] CMOS RTC initialized: %04u-%02u-%02u %02u:%02u:%02u\n",
              g_boot_year, g_boot_month, g_boot_day,
              g_boot_hours, g_boot_minutes, g_boot_seconds);
}

static uint64_t clock_total_seconds(void) {
    return g_seconds_at_boot + (HalGetTickCount() / 1000ULL);
}

uint8_t HalClockSeconds(void) {
    return (uint8_t)(clock_total_seconds() % 60ULL);
}

uint8_t HalClockMinutes(void) {
    return (uint8_t)((clock_total_seconds() / 60ULL) % 60ULL);
}

uint8_t HalClockHours(void) {
    return (uint8_t)((clock_total_seconds() / 3600ULL) % 24ULL);
}

uint8_t HalClockDay(void) {
    return g_boot_day;
}

uint8_t HalClockMonth(void) {
    return g_boot_month;
}

uint16_t HalClockYear(void) {
    return g_boot_year;
}

void HalClockGetTime(DIMA_TIME* out) {
    if (!out) return;
    uint64_t total = clock_total_seconds();
    out->seconds = (uint8_t)(total % 60ULL);
    out->minutes = (uint8_t)((total / 60ULL) % 60ULL);
    out->hours   = (uint8_t)((total / 3600ULL) % 24ULL);
    out->day     = g_boot_day;
    out->month   = g_boot_month;
    out->year    = g_boot_year;
}
