#include <stdbool.h>
#include <stdio.h>

#include "domain/rtc_time.h"

static void check(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        _Exit(1);
    }
}

static void test_bcd(void) {
    uint8_t value = 0;
    check(rtc_bcd_decode(0x00, 59, &value) && value == 0, "BCD zero decodes");
    check(rtc_bcd_decode(0x59, 59, &value) && value == 59, "BCD 59 decodes");
    check(rtc_bcd_encode(42) == 0x42, "42 encodes as BCD");
    check(!rtc_bcd_decode(0x6A, 99, &value), "invalid BCD digit rejected");
    check(!rtc_bcd_decode(0x60, 59, &value), "BCD value above bound rejected");
    check(!rtc_bcd_decode(0x11, 10, &value), "strict maximum is enforced");
    check(!rtc_bcd_decode(0x10, 10, NULL), "NULL BCD output rejected");
}

static void test_calendar(void) {
    check(rtc_is_leap_year(2024), "2024 is a leap year");
    check(!rtc_is_leap_year(2100), "2100 is outside supported leap range");
    check(rtc_days_in_month(2024, 2) == 29, "leap February has 29 days");
    check(rtc_days_in_month(2023, 2) == 28, "normal February has 28 days");
    check(rtc_days_in_month(2024, 13) == 0, "invalid month has no days");

    const rtc_datetime_t valid = {
        .year = 2024,
        .month = 2,
        .day = 29,
        .weekday = 5,
        .hour = 23,
        .minute = 59,
        .second = 59,
    };
    check(rtc_datetime_is_valid(&valid), "leap-day datetime is valid");

    rtc_datetime_t invalid = valid;
    invalid.day = 30;
    check(!rtc_datetime_is_valid(&invalid), "day past February rejected");
    invalid = valid;
    invalid.hour = 24;
    check(!rtc_datetime_is_valid(&invalid), "hour 24 rejected");
    invalid = valid;
    invalid.weekday = 0;
    check(!rtc_datetime_is_valid(&invalid), "weekday zero rejected");
    invalid = valid;
    invalid.year = 1999;
    check(!rtc_datetime_is_valid(&invalid),
          "year before DS3231 range rejected");
    check(!rtc_datetime_is_valid(NULL), "NULL datetime rejected");
}

int main(void) {
    test_bcd();
    test_calendar();
    puts("RTC tests: OK");
    return 0;
}
