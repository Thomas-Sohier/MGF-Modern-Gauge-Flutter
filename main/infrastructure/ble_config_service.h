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

// Lien BLE unique de la jauge : service de réglages (0111b043-…) et service
// de l'application compagnon (7f3a0001-…, cf. companion_gatt.h).
//
// Politique : la jauge annonce en permanence pour que le téléphone appairé se
// reconnecte seul au démarrage, mais seul un téléphone appairé (bond stocké
// en NVS) garde la connexion. Un nouvel appairage n'est possible que pendant
// la fenêtre ouverte depuis l'écran de réglages (ou au boot sur une image de
// banc avec pairing_open_on_start).
typedef struct {
    const char *device_name;
    void *settings_context;
    ble_config_settings_read_cb settings_read;
    ble_config_settings_update_cb settings_update;
    void *rtc_context;
    ble_config_datetime_set_cb datetime_set;
    bool pairing_open_on_start;
} ble_config_service_config_t;

// Initializes NimBLE, registers both services, enables bonded encrypted
// writes, starts advertising and the NimBLE host task.
esp_err_t ble_config_service_start(const ble_config_service_config_t *config);

bool ble_config_service_is_started(void);
// Lectures d'état pour l'UI (tâche LVGL) ; valeurs indicatives, écrites par
// la tâche NimBLE.
bool ble_config_service_is_pairing_open(void);
// Un téléphone appairé est connecté, lien chiffré avec sa clé.
bool ble_config_service_phone_linked(void);
// Fenêtre d'appairage : autorise la création d'un bond. La fermeture coupe un
// lien encore non authentifié mais garde le téléphone appairé connecté.
esp_err_t ble_config_service_open_pairing(void);
esp_err_t ble_config_service_close_pairing(void);

// Deletes every NimBLE bond. Call only while pairing is closed and no
// connection is active. Every phone will then need to pair again.
esp_err_t ble_config_service_forget_bonds(void);

// Settings written over BLE are validated by settings_update and then queued
// here. The application task consumes the snapshot, applies it through its
// coordinator, and performs the debounced persistence. It calls
// ble_config_service_sync_settings() after local changes. This avoids calling
// LVGL or application state from NimBLE's task.
bool ble_config_service_take_settings_update(app_settings_t *settings);
esp_err_t ble_config_service_sync_settings(const app_settings_t *settings);
