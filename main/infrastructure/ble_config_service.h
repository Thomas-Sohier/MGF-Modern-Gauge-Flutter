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

// Policy limits are also reflected in sdkconfig.defaults. They are kept here
// so a client and the firmware have an explicit, reviewable contract.
#define BLE_CONFIG_DEVICE_NAME_MAX_LENGTH 31U
#define BLE_CONFIG_MAX_CONNECTIONS 1U
#define BLE_CONFIG_MAX_BONDS 3U
#define BLE_CONFIG_REQUIRE_BONDING 1U
#define BLE_CONFIG_REQUIRE_MITM 0U
#define BLE_CONFIG_REQUIRE_SECURE_CONNECTIONS 1U

// The service is closed by default. Set open_on_start only for a controlled
// bench/service image; production code should call ble_config_service_open()
// after an explicit local maintenance action.
typedef struct {
    const char *device_name;
    void *settings_context;
    ble_config_settings_read_cb settings_read;
    ble_config_settings_update_cb settings_update;
    void *rtc_context;
    ble_config_datetime_set_cb datetime_set;
    bool open_on_start;
} ble_config_service_config_t;

// Initializes NimBLE, registers the version-1 service, enables bonded
// encrypted writes, and starts the NimBLE host task. The service does not
// advertise unless open_on_start is true or ble_config_service_open() is
// called explicitly.
esp_err_t ble_config_service_start(const ble_config_service_config_t *config);

bool ble_config_service_is_started(void);
esp_err_t ble_config_service_open(void);
esp_err_t ble_config_service_close(void);

// Deletes every NimBLE bond. Call only while the service is closed and no
// connection is active. The next explicit open will require fresh pairing.
esp_err_t ble_config_service_forget_bonds(void);

// Settings written over BLE are validated by settings_update and then queued
// here. The application task consumes the snapshot, applies it through its
// coordinator, and performs the debounced persistence. It calls
// ble_config_service_sync_settings() after local changes. This avoids calling
// LVGL or application state from NimBLE's task.
bool ble_config_service_take_settings_update(app_settings_t *settings);
esp_err_t ble_config_service_sync_settings(const app_settings_t *settings);
