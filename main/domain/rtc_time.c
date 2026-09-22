#include "domain/rtc_time.h"

#include <stddef.h>

static bool valid_year(uint16_t year) {
    return year >= 2000 && year <= 2099;
}

bool rtc_bcd_decode(uint8_t encoded, uint8_t max_value, uint8_t *out) {
    const uint8_t ones = encoded & 0x0Fu;
    const uint8_t tens = (encoded >> 4) & 0x0Fu;
    if (ones > 9 || tens > 9) return false;

    const uint8_t value = (uint8_t)(tens * 10u + ones);
    if (value > max_value || out == NULL) return false;
    *out = value;
    return true;
}

uint8_t rtc_bcd_encode(uint8_t value) {
    return (uint8_t)(((value / 10u) << 4) | (value % 10u));
}

bool rtc_is_leap_year(uint16_t year) {
    return valid_year(year) && (year % 4u == 0u);
}

uint8_t rtc_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {
        0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if (!valid_year(year) || month < 1 || month > 12) return 0;
    if (month == 2 && rtc_is_leap_year(year)) return 29;
    return days[month];
}

bool rtc_datetime_is_valid(const rtc_datetime_t *date_time) {
    if (date_time == NULL || !valid_year(date_time->year) ||
        date_time->month < 1 || date_time->month > 12 ||
        date_time->day < 1 ||
        date_time->day > rtc_days_in_month(date_time->year, date_time->month) ||
        date_time->weekday < 1 || date_time->weekday > 7 ||
        date_time->hour > 23 || date_time->minute > 59 ||
        date_time->second > 59) {
        return false;
    }
    return true;
}

const char *rtc_result_name(rtc_result_t result) {
    switch (result) {
    case RTC_OK: return "ok";
    case RTC_ERR_INVALID_ARGUMENT: return "invalid-argument";
    case RTC_ERR_IO: return "io";
    case RTC_ERR_OSCILLATOR_STOPPED: return "oscillator-stopped";
    case RTC_ERR_INVALID_TIME: return "invalid-time";
    default: return "unknown";
    }
}
