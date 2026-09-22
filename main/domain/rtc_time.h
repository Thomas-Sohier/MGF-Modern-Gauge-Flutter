#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "domain/rtc.h"

// Décode un octet BCD strictement dans 0..max_value.
bool rtc_bcd_decode(uint8_t encoded, uint8_t max_value, uint8_t *out);
uint8_t rtc_bcd_encode(uint8_t value);

// Validation civile grégorienne pour la plage supportée par le DS3231.
bool rtc_is_leap_year(uint16_t year);
uint8_t rtc_days_in_month(uint16_t year, uint8_t month);
