#include "domain/ble_config_protocol.h"

#include <stddef.h>

static bool valid_time_basis(uint8_t basis) {
    return basis == BLE_CONFIG_TIME_UTC || basis == BLE_CONFIG_TIME_LOCAL;
}

static uint16_t read_u16_le(const uint8_t *bytes) {
    return (uint16_t)bytes[0] | (uint16_t)((uint16_t)bytes[1] << 8);
}

static void write_u16_le(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value & 0xffu);
    bytes[1] = (uint8_t)(value >> 8);
}

bool ble_config_settings_encode(const app_settings_t *settings,
                                uint8_t *payload, size_t capacity,
                                size_t *payload_size) {
    if (settings == NULL || payload == NULL || payload_size == NULL ||
        capacity < BLE_CONFIG_SETTINGS_PAYLOAD_SIZE ||
        !app_settings_is_valid(settings)) {
        return false;
    }

    payload[0] = BLE_CONFIG_PROTOCOL_VERSION;
    payload[1] = settings->brightness_percent;
    payload[2] = (uint8_t)settings->selected_page;
    payload[3] = (uint8_t)settings->theme;
    payload[4] = (uint8_t)settings->units;
    *payload_size = BLE_CONFIG_SETTINGS_PAYLOAD_SIZE;
    return true;
}

ble_config_parse_result_t ble_config_settings_decode(
    const uint8_t *payload, size_t payload_size, app_settings_t *settings) {
    if (payload == NULL || settings == NULL) {
        return BLE_CONFIG_PARSE_INVALID_ARGUMENT;
    }
    if (payload_size != BLE_CONFIG_SETTINGS_PAYLOAD_SIZE) {
        return BLE_CONFIG_PARSE_INVALID_LENGTH;
    }
    if (payload[0] != BLE_CONFIG_PROTOCOL_VERSION) {
        return BLE_CONFIG_PARSE_UNSUPPORTED_VERSION;
    }

    const app_settings_t decoded = {
        .brightness_percent = payload[1],
        .selected_page = (app_settings_page_t)payload[2],
        .theme = (app_settings_theme_t)payload[3],
        .units = (app_settings_units_t)payload[4],
        // Absent du protocole v1 : l'application conserve sa valeur locale.
        .startup_page = APP_SETTINGS_STARTUP_LAST_PAGE,
    };
    if (!app_settings_is_valid(&decoded)) {
        return BLE_CONFIG_PARSE_INVALID_VALUE;
    }

    *settings = decoded;
    return BLE_CONFIG_PARSE_OK;
}

bool ble_config_datetime_encode(const ble_config_datetime_t *datetime,
                                uint8_t *payload, size_t capacity,
                                size_t *payload_size) {
    if (datetime == NULL || payload == NULL || payload_size == NULL ||
        capacity < BLE_CONFIG_DATETIME_PAYLOAD_SIZE ||
        !valid_time_basis(datetime->basis) ||
        !rtc_datetime_is_valid(&datetime->date_time)) {
        return false;
    }

    payload[0] = BLE_CONFIG_PROTOCOL_VERSION;
    payload[1] = (uint8_t)datetime->basis;
    write_u16_le(&payload[2], datetime->date_time.year);
    payload[4] = datetime->date_time.month;
    payload[5] = datetime->date_time.day;
    payload[6] = datetime->date_time.weekday;
    payload[7] = datetime->date_time.hour;
    payload[8] = datetime->date_time.minute;
    payload[9] = datetime->date_time.second;
    *payload_size = BLE_CONFIG_DATETIME_PAYLOAD_SIZE;
    return true;
}

ble_config_parse_result_t ble_config_datetime_decode(
    const uint8_t *payload, size_t payload_size,
    ble_config_datetime_t *datetime) {
    if (payload == NULL || datetime == NULL) {
        return BLE_CONFIG_PARSE_INVALID_ARGUMENT;
    }
    if (payload_size != BLE_CONFIG_DATETIME_PAYLOAD_SIZE) {
        return BLE_CONFIG_PARSE_INVALID_LENGTH;
    }
    if (payload[0] != BLE_CONFIG_PROTOCOL_VERSION) {
        return BLE_CONFIG_PARSE_UNSUPPORTED_VERSION;
    }
    if (!valid_time_basis(payload[1])) {
        return BLE_CONFIG_PARSE_INVALID_VALUE;
    }

    const ble_config_datetime_t decoded = {
        .basis = (ble_config_time_basis_t)payload[1],
        .date_time = {
            .year = read_u16_le(&payload[2]),
            .month = payload[4],
            .day = payload[5],
            .weekday = payload[6],
            .hour = payload[7],
            .minute = payload[8],
            .second = payload[9],
        },
    };
    if (!rtc_datetime_is_valid(&decoded.date_time)) {
        return BLE_CONFIG_PARSE_INVALID_VALUE;
    }

    *datetime = decoded;
    return BLE_CONFIG_PARSE_OK;
}

const char *ble_config_parse_result_name(ble_config_parse_result_t result) {
    switch (result) {
    case BLE_CONFIG_PARSE_OK: return "ok";
    case BLE_CONFIG_PARSE_INVALID_ARGUMENT: return "invalid-argument";
    case BLE_CONFIG_PARSE_INVALID_LENGTH: return "invalid-length";
    case BLE_CONFIG_PARSE_UNSUPPORTED_VERSION: return "unsupported-version";
    case BLE_CONFIG_PARSE_INVALID_VALUE: return "invalid-value";
    default: return "unknown";
    }
}
