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

// Conversions secondes Unix (UTC) <-> date civile, limitées à 2000..2099.
// Le jour de semaine est recalculé (1 = dimanche). false hors plage.
bool rtc_datetime_from_unix(int64_t unix_seconds, rtc_datetime_t *out);
bool rtc_datetime_to_unix(const rtc_datetime_t *date_time,
                          int64_t *unix_seconds);
// Heure locale = UTC + décalage (minutes, peut changer de jour). false si la
// date source est invalide ou si le résultat sort de 2000..2099.
bool rtc_datetime_add_minutes(const rtc_datetime_t *utc, int32_t minutes,
                              rtc_datetime_t *out);
