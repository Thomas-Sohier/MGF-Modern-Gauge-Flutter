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

// Jours depuis 1970-01-01 pour une date grégorienne (algorithme de Howard
// Hinnant, exact pour toute date positive).
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static void civil_from_days(int64_t z, int64_t *y, unsigned *m, unsigned *d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = (unsigned)(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;
    *d = doy - (153u * mp + 2u) / 5u + 1u;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = (int64_t)yoe + era * 400 + (*m <= 2);
}

bool rtc_datetime_from_unix(int64_t unix_seconds, rtc_datetime_t *out) {
    if (out == NULL || unix_seconds < 0) return false;
    const int64_t days = unix_seconds / 86400;
    const int64_t rem = unix_seconds % 86400;
    int64_t year;
    unsigned month;
    unsigned day;
    civil_from_days(days, &year, &month, &day);
    if (!valid_year((uint16_t)(year < 0 || year > 65535 ? 0 : year))) {
        return false;
    }
    *out = (rtc_datetime_t){
        .year = (uint16_t)year,
        .month = (uint8_t)month,
        .day = (uint8_t)day,
        // 1970-01-01 était un jeudi (5 en convention 1 = dimanche).
        .weekday = (uint8_t)((days + 4) % 7 + 1),
        .hour = (uint8_t)(rem / 3600),
        .minute = (uint8_t)((rem / 60) % 60),
        .second = (uint8_t)(rem % 60),
    };
    return true;
}

bool rtc_datetime_to_unix(const rtc_datetime_t *date_time,
                          int64_t *unix_seconds) {
    if (date_time == NULL || unix_seconds == NULL ||
        !valid_year(date_time->year) || date_time->month < 1 ||
        date_time->month > 12 || date_time->day < 1 ||
        date_time->day > rtc_days_in_month(date_time->year, date_time->month) ||
        date_time->hour > 23 || date_time->minute > 59 ||
        date_time->second > 59) {
        return false;
    }
    const int64_t days = days_from_civil(date_time->year, date_time->month,
                                         date_time->day);
    *unix_seconds = days * 86400 + date_time->hour * 3600 +
                    date_time->minute * 60 + date_time->second;
    return true;
}

bool rtc_datetime_add_minutes(const rtc_datetime_t *utc, int32_t minutes,
                              rtc_datetime_t *out) {
    int64_t seconds;
    if (out == NULL || !rtc_datetime_to_unix(utc, &seconds)) return false;
    return rtc_datetime_from_unix(seconds + (int64_t)minutes * 60, out);
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
