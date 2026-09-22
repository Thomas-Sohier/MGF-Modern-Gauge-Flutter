#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "domain/app_settings.h"
#include "domain/rtc.h"

#define BLE_CONFIG_PROTOCOL_VERSION 1U
#define BLE_CONFIG_SETTINGS_PAYLOAD_SIZE 5U
#define BLE_CONFIG_DATETIME_PAYLOAD_SIZE 10U

typedef enum {
    BLE_CONFIG_PARSE_OK = 0,
    BLE_CONFIG_PARSE_INVALID_ARGUMENT,
    BLE_CONFIG_PARSE_INVALID_LENGTH,
    BLE_CONFIG_PARSE_UNSUPPORTED_VERSION,
    BLE_CONFIG_PARSE_INVALID_VALUE,
} ble_config_parse_result_t;

typedef enum {
    BLE_CONFIG_TIME_UTC = 0,
    BLE_CONFIG_TIME_LOCAL = 1,
} ble_config_time_basis_t;

typedef struct {
    rtc_datetime_t date_time;
    ble_config_time_basis_t basis;
} ble_config_datetime_t;

// Wire format is deliberately fixed-width and little-endian. These functions
// do not allocate memory, perform I/O, or depend on ESP-IDF/NimBLE.
bool ble_config_settings_encode(const app_settings_t *settings,
                                uint8_t *payload, size_t capacity,
                                size_t *payload_size);
ble_config_parse_result_t ble_config_settings_decode(
    const uint8_t *payload, size_t payload_size, app_settings_t *settings);

bool ble_config_datetime_encode(const ble_config_datetime_t *datetime,
                                uint8_t *payload, size_t capacity,
                                size_t *payload_size);
ble_config_parse_result_t ble_config_datetime_decode(
    const uint8_t *payload, size_t payload_size,
    ble_config_datetime_t *datetime);

const char *ble_config_parse_result_name(ble_config_parse_result_t result);
