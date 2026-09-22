#pragma once

#include "domain/app_settings.h"
#include "esp_err.h"

// Initializes the shared NVS partition and opens the application namespace.
// This function does not erase NVS on recovery errors: doing so would also
// remove Bluetooth bonds owned by the ESP-IDF Bluetooth stack.
esp_err_t settings_store_init(void);

// Loads settings into out. Missing, unsupported, or invalid application data
// is treated as defaults and is not written back during load. Unexpected NVS
// read errors are returned and make saving unavailable until a later load.
esp_err_t settings_store_load(app_settings_t *out);

// Saves a changed settings snapshot. Call after settings_store_load(); an
// identical snapshot returns ESP_OK without any NVS write or commit. Calls
// are serialized and all keys are committed as one NVS transaction.
esp_err_t settings_store_save(const app_settings_t *settings);
