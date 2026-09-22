#include "infrastructure/rtc_ds3231.h"

#include "domain/rtc_time.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define DS3231_REG_SECONDS 0x00u
#define DS3231_REG_STATUS  0x0Fu
#define DS3231_STATUS_OSF  0x80u
#define DS3231_REGISTER_COUNT 0x10u

struct rtc_s {
    shared_i2c_bus_t bus;
    uint8_t address;
    uint32_t timeout_ms;
};

static rtc_result_t map_i2c_error(esp_err_t error) {
    return error == ESP_OK ? RTC_OK : RTC_ERR_IO;
}

static rtc_result_t read_registers(const rtc_t *rtc, uint8_t start,
                                   uint8_t *data, size_t size) {
    if (rtc == NULL || data == NULL || size == 0 || rtc->bus.write_read == NULL) {
        return RTC_ERR_INVALID_ARGUMENT;
    }
    const esp_err_t error = rtc->bus.write_read(
        rtc->bus.context, rtc->address, &start, 1, data, size, rtc->timeout_ms);
    return map_i2c_error(error);
}

static rtc_result_t write_registers(const rtc_t *rtc, uint8_t start,
                                    const uint8_t *data, size_t size) {
    if (rtc == NULL || data == NULL || size == 0 || rtc->bus.write == NULL) {
        return RTC_ERR_INVALID_ARGUMENT;
    }

    uint8_t buffer[1 + 7];
    if (size > sizeof(buffer) - 1) return RTC_ERR_INVALID_ARGUMENT;
    buffer[0] = start;
    memcpy(&buffer[1], data, size);
    const esp_err_t error = rtc->bus.write(
        rtc->bus.context, rtc->address, buffer, size + 1, rtc->timeout_ms);
    return map_i2c_error(error);
}

static bool decode_time(const uint8_t *registers, rtc_datetime_t *out) {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t month;
    uint8_t year;

    if (!rtc_bcd_decode(registers[0] & 0x7Fu, 59, &seconds) ||
        !rtc_bcd_decode(registers[1] & 0x7Fu, 59, &minutes) ||
        !rtc_bcd_decode(registers[4] & 0x3Fu, 31, &day) ||
        !rtc_bcd_decode(registers[5] & 0x1Fu, 12, &month) ||
        !rtc_bcd_decode(registers[6], 99, &year) ||
        (registers[2] & 0x80u) != 0u || (registers[3] & 0xF8u) != 0u ||
        (registers[4] & 0xC0u) != 0u || (registers[5] & 0x60u) != 0u ||
        (registers[5] & 0x80u) != 0u) {
        return false;
    }

    if ((registers[2] & 0x40u) != 0u) {
        const uint8_t hour_bcd = registers[2] & 0x1Fu;
        uint8_t hour_12;
        if (!rtc_bcd_decode(hour_bcd, 12, &hour_12) || hour_12 == 0) return false;
        const bool pm = (registers[2] & 0x20u) != 0u;
        hours = (uint8_t)(hour_12 % 12u + (pm ? 12u : 0u));
    } else if (!rtc_bcd_decode(registers[2] & 0x3Fu, 23, &hours)) {
        return false;
    }

    *out = (rtc_datetime_t){
        .year = (uint16_t)(2000u + year),
        .month = month,
        .day = day,
        .weekday = registers[3] & 0x07u,
        .hour = hours,
        .minute = minutes,
        .second = seconds,
    };
    return rtc_datetime_is_valid(out);
}

static void encode_time(const rtc_datetime_t *date_time, uint8_t *registers) {
    registers[0] = rtc_bcd_encode(date_time->second);
    registers[1] = rtc_bcd_encode(date_time->minute);
    registers[2] = rtc_bcd_encode(date_time->hour); // mode 24 h
    registers[3] = date_time->weekday;
    registers[4] = rtc_bcd_encode(date_time->day);
    registers[5] = rtc_bcd_encode(date_time->month);
    registers[6] = rtc_bcd_encode((uint8_t)(date_time->year - 2000u));
}

rtc_t *ds3231_create(const ds3231_config_t *config) {
    if (config == NULL ||
        (config->address != 0 && config->address != DS3231_I2C_ADDRESS) ||
        config->bus.write == NULL ||
        config->bus.write_read == NULL ||
        // This guard prevents accidentally probing the current Waveshare bus.
        // The LILYGO display branch must export the GPIO8/GPIO48 bus here.
        config->bus.sda_gpio != DS3231_I2C_SDA_GPIO ||
        config->bus.scl_gpio != DS3231_I2C_SCL_GPIO) {
        return NULL;
    }

    rtc_t *rtc = calloc(1, sizeof(*rtc));
    if (rtc == NULL) return NULL;
    rtc->bus = config->bus;
    rtc->address = config->address == 0 ? DS3231_I2C_ADDRESS : config->address;
    rtc->timeout_ms = config->timeout_ms;
    return rtc;
}

rtc_result_t rtc_probe(rtc_t *rtc) {
    if (rtc == NULL) return RTC_ERR_INVALID_ARGUMENT;

    uint8_t registers[DS3231_REGISTER_COUNT];
    const rtc_result_t result = read_registers(
        rtc, DS3231_REG_SECONDS, registers, sizeof(registers));
    if (result != RTC_OK) return result;
    if ((registers[DS3231_REG_STATUS] & DS3231_STATUS_OSF) != 0u) {
        return RTC_ERR_OSCILLATOR_STOPPED;
    }
    return decode_time(registers, &(rtc_datetime_t){0})
               ? RTC_OK
               : RTC_ERR_INVALID_TIME;
}

rtc_result_t rtc_read(rtc_t *rtc, rtc_datetime_t *out) {
    if (rtc == NULL || out == NULL) return RTC_ERR_INVALID_ARGUMENT;

    uint8_t registers[DS3231_REGISTER_COUNT];
    const rtc_result_t result = read_registers(
        rtc, DS3231_REG_SECONDS, registers, sizeof(registers));
    if (result != RTC_OK) return result;
    if ((registers[DS3231_REG_STATUS] & DS3231_STATUS_OSF) != 0u) {
        return RTC_ERR_OSCILLATOR_STOPPED;
    }
    return decode_time(registers, out) ? RTC_OK : RTC_ERR_INVALID_TIME;
}

rtc_result_t rtc_set(rtc_t *rtc, const rtc_datetime_t *date_time) {
    if (rtc == NULL || date_time == NULL) return RTC_ERR_INVALID_ARGUMENT;
    if (!rtc_datetime_is_valid(date_time)) return RTC_ERR_INVALID_TIME;

    uint8_t status;
    rtc_result_t result = read_registers(rtc, DS3231_REG_STATUS, &status, 1);
    if (result != RTC_OK) return result;

    uint8_t registers[7];
    encode_time(date_time, registers);
    result = write_registers(rtc, DS3231_REG_SECONDS, registers, sizeof(registers));
    if (result != RTC_OK) return result;

    status &= (uint8_t)~DS3231_STATUS_OSF;
    return write_registers(rtc, DS3231_REG_STATUS, &status, 1);
}

void rtc_destroy(rtc_t *rtc) {
    free(rtc);
}
