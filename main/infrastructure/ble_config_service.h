#pragma once

#include <stdbool.h>

#include "domain/app_settings.h"
#include "domain/ble_config_protocol.h"
#include "domain/rtc.h"
#include "esp_err.h"

// The service owns neither settings storage nor the RTC. Callbacks execute in
// the NimBLE host task and must be short and safe to call from that context.
typedef bool (*ble_config_settings_read_cb)(void *context,
                                             app_settings_t *settings);
typedef bool (*ble_config_settings_update_cb)(void *context,
                                               const app_settings_t *settings);
typedef rtc_result_t (*ble_config_datetime_set_cb)(
    void *context, const rtc_datetime_t *date_time,
    ble_config_time_basis_t basis);

typedef struct {
    const char *device_name;
    void *settings_context;
    ble_config_settings_read_cb settings_read;
    ble_config_settings_update_cb settings_update;
    void *rtc_context;
    ble_config_datetime_set_cb datetime_set;
} ble_config_service_config_t;

// Initializes NimBLE, registers the version-1 service, enables bonded
// encrypted writes, and starts the NimBLE host task. There is no stop API:
// this service is started once for the lifetime of the firmware.
esp_err_t ble_config_service_start(const ble_config_service_config_t *config);
