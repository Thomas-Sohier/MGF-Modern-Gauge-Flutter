#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "domain/ble_config_protocol.h"

static void test_settings_round_trip(void) {
    app_settings_t source;
    app_settings_defaults(&source);
    source.brightness_percent = 42;
    source.selected_page = APP_SETTINGS_PAGE_LAMBDA;
    source.units = APP_SETTINGS_UNITS_IMPERIAL;

    uint8_t payload[BLE_CONFIG_SETTINGS_PAYLOAD_SIZE];
    size_t payload_size = 0;
    assert(ble_config_settings_encode(&source, payload, sizeof(payload),
                                      &payload_size));
    assert(payload_size == sizeof(payload));

    app_settings_t decoded = {0};
    assert(ble_config_settings_decode(payload, payload_size, &decoded) ==
           BLE_CONFIG_PARSE_OK);
    assert(app_settings_equal(&source, &decoded));
}

static void test_settings_rejects_malformed_payloads(void) {
    app_settings_t decoded;
    uint8_t payload[BLE_CONFIG_SETTINGS_PAYLOAD_SIZE] = {
        BLE_CONFIG_PROTOCOL_VERSION, 50,
        APP_SETTINGS_PAGE_RPM,       APP_SETTINGS_THEME_AMBER,
        APP_SETTINGS_UNITS_METRIC,
    };
    assert(ble_config_settings_decode(payload, sizeof(payload) - 1, &decoded) ==
           BLE_CONFIG_PARSE_INVALID_LENGTH);
    payload[0] = BLE_CONFIG_PROTOCOL_VERSION + 1U;
    assert(ble_config_settings_decode(payload, sizeof(payload), &decoded) ==
           BLE_CONFIG_PARSE_UNSUPPORTED_VERSION);
    payload[0] = BLE_CONFIG_PROTOCOL_VERSION;
    payload[1] = 101;
    assert(ble_config_settings_decode(payload, sizeof(payload), &decoded) ==
           BLE_CONFIG_PARSE_INVALID_VALUE);
    payload[1] = 50;
    payload[2] = APP_SETTINGS_PAGE_COUNT;
    assert(ble_config_settings_decode(payload, sizeof(payload), &decoded) ==
           BLE_CONFIG_PARSE_INVALID_VALUE);
    payload[2] = APP_SETTINGS_PAGE_RPM;
    payload[3] = APP_SETTINGS_THEME_COUNT;
    assert(ble_config_settings_decode(payload, sizeof(payload), &decoded) ==
           BLE_CONFIG_PARSE_INVALID_VALUE);
    payload[3] = APP_SETTINGS_THEME_AMBER;
    payload[4] = APP_SETTINGS_UNITS_COUNT;
    assert(ble_config_settings_decode(payload, sizeof(payload), &decoded) ==
           BLE_CONFIG_PARSE_INVALID_VALUE);
    assert(ble_config_settings_decode(NULL, sizeof(payload), &decoded) ==
           BLE_CONFIG_PARSE_INVALID_ARGUMENT);
}

static void test_datetime_round_trip(void) {
    const ble_config_datetime_t source = {
        .basis = BLE_CONFIG_TIME_LOCAL,
        .date_time =
            {
                .year = 2024,
                .month = 2,
                .day = 29,
                .weekday = 5,
                .hour = 23,
                .minute = 59,
                .second = 58,
            },
    };
    uint8_t payload[BLE_CONFIG_DATETIME_PAYLOAD_SIZE];
    size_t payload_size = 0;
    assert(ble_config_datetime_encode(&source, payload, sizeof(payload),
                                      &payload_size));
    assert(payload[2] == 0xe8 && payload[3] == 0x07);

    ble_config_datetime_t decoded = {0};
    assert(ble_config_datetime_decode(payload, payload_size, &decoded) ==
           BLE_CONFIG_PARSE_OK);
    assert(decoded.basis == source.basis);
    assert(memcmp(&decoded.date_time, &source.date_time,
                  sizeof(source.date_time)) == 0);
}

static void test_datetime_rejects_malformed_payloads(void) {
    ble_config_datetime_t decoded;
    uint8_t payload[BLE_CONFIG_DATETIME_PAYLOAD_SIZE] = {
        BLE_CONFIG_PROTOCOL_VERSION,
        BLE_CONFIG_TIME_UTC,
        0xe8,
        0x07,
        2,
        29,
        5,
        23,
        59,
        59,
    };
    assert(ble_config_datetime_decode(payload, sizeof(payload) - 1, &decoded) ==
           BLE_CONFIG_PARSE_INVALID_LENGTH);
    payload[0] = 0;
    assert(ble_config_datetime_decode(payload, sizeof(payload), &decoded) ==
           BLE_CONFIG_PARSE_UNSUPPORTED_VERSION);
    payload[0] = BLE_CONFIG_PROTOCOL_VERSION;
    payload[1] = 2;
    assert(ble_config_datetime_decode(payload, sizeof(payload), &decoded) ==
           BLE_CONFIG_PARSE_INVALID_VALUE);
    payload[1] = BLE_CONFIG_TIME_UTC;
    payload[5] = 30;
    assert(ble_config_datetime_decode(payload, sizeof(payload), &decoded) ==
           BLE_CONFIG_PARSE_INVALID_VALUE);
    assert(!ble_config_datetime_encode(
        &((ble_config_datetime_t){
            .basis = BLE_CONFIG_TIME_UTC,
            .date_time = {.year = 2023, .month = 2, .day = 29, .weekday = 3},
        }),
        payload, sizeof(payload), &(size_t){0}));
    assert(ble_config_datetime_decode(NULL, sizeof(payload), &decoded) ==
           BLE_CONFIG_PARSE_INVALID_ARGUMENT);
}

int main(void) {
    test_settings_round_trip();
    test_settings_rejects_malformed_payloads();
    test_datetime_round_trip();
    test_datetime_rejects_malformed_payloads();
    puts("BLE config protocol tests: OK");
    return 0;
}
