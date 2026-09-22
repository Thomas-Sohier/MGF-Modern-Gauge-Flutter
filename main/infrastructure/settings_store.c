#include "infrastructure/settings_store.h"

#include <stddef.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "settings";

#define SETTINGS_NAMESPACE "app_settings"
#define KEY_SCHEMA "schema"
#define KEY_BRIGHTNESS "brightness"
#define KEY_PAGE "page"
#define KEY_THEME "theme"
#define KEY_UNITS "units"

static nvs_handle_t s_handle;
static app_settings_t s_loaded;
static SemaphoreHandle_t s_mutex;
static bool s_initialized;
static bool s_has_loaded;

static void use_defaults(app_settings_t *out, const char *reason) {
    app_settings_defaults(out);
    ESP_LOGW(TAG, "using default settings (%s)", reason);
}

static bool is_invalid_record_error(esp_err_t error) {
    return error == ESP_ERR_NVS_NOT_FOUND ||
           error == ESP_ERR_NVS_TYPE_MISMATCH ||
           error == ESP_ERR_NVS_INVALID_LENGTH;
}

static esp_err_t finish_with_defaults(app_settings_t *out, const char *reason) {
    use_defaults(out, reason);
    s_loaded = *out;
    s_has_loaded = true;
    return ESP_OK;
}

static bool lock_store(void) {
    return s_mutex != NULL && xSemaphoreTake(s_mutex, portMAX_DELAY) == pdTRUE;
}

static void unlock_store(void) {
    (void)xSemaphoreGive(s_mutex);
}

static esp_err_t read_u8(nvs_handle_t handle, const char *key, uint8_t *value) {
    return nvs_get_u8(handle, key, value);
}

esp_err_t settings_store_init(void) {
    if (s_initialized) return ESP_OK;

    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        // Deliberately do not call nvs_flash_erase(). The NVS partition also
        // contains Bluetooth bonding data managed by ESP-IDF.
        ESP_LOGE(TAG, "NVS unavailable: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cannot open namespace: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t settings_store_load(app_settings_t *out) {
    if (out == NULL) return ESP_ERR_INVALID_ARG;

    app_settings_defaults(out);
    if (!s_initialized || !lock_store()) return ESP_ERR_INVALID_STATE;

    // A failed reload must not leave a previous valid snapshot available for
    // a later save.
    s_has_loaded = false;

    uint32_t schema = 0;
    esp_err_t err = nvs_get_u32(s_handle, KEY_SCHEMA, &schema);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = finish_with_defaults(out, "first boot");
        unlock_store();
        return err;
    }
    if (err != ESP_OK) {
        if (is_invalid_record_error(err)) {
            err = finish_with_defaults(out, "schema is invalid");
            unlock_store();
            return err;
        }
        ESP_LOGE(TAG, "cannot read settings schema: %s", esp_err_to_name(err));
        unlock_store();
        return err;
    }
    if (schema != APP_SETTINGS_SCHEMA_VERSION) {
        // Schema 1 is the first deployed representation; do not guess a
        // migration for an unknown record. Add an explicit case when a
        // future, documented legacy representation exists.
        err = finish_with_defaults(out, "unsupported schema");
        unlock_store();
        return err;
    }

    uint8_t brightness = 0;
    uint8_t page = 0;
    uint8_t theme = 0;
    uint8_t units = 0;
    err = read_u8(s_handle, KEY_BRIGHTNESS, &brightness);
    if (err == ESP_OK) err = read_u8(s_handle, KEY_PAGE, &page);
    if (err == ESP_OK) err = read_u8(s_handle, KEY_THEME, &theme);
    if (err == ESP_OK) err = read_u8(s_handle, KEY_UNITS, &units);
    if (err != ESP_OK) {
        if (is_invalid_record_error(err)) {
            err = finish_with_defaults(out, "settings are incomplete or invalid");
            unlock_store();
            return err;
        }
        ESP_LOGE(TAG, "cannot read settings values: %s", esp_err_to_name(err));
        unlock_store();
        return err;
    }

    *out = (app_settings_t){
        .brightness_percent = brightness,
        .selected_page = (app_settings_page_t)page,
        .theme = (app_settings_theme_t)theme,
        .units = (app_settings_units_t)units,
    };
    if (!app_settings_is_valid(out)) {
        err = finish_with_defaults(out, "settings are invalid");
        unlock_store();
        return err;
    }

    s_loaded = *out;
    s_has_loaded = true;
    unlock_store();
    return ESP_OK;
}

esp_err_t settings_store_save(const app_settings_t *settings) {
    if (settings == NULL || !app_settings_is_valid(settings)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !lock_store()) return ESP_ERR_INVALID_STATE;
    if (!s_has_loaded) {
        unlock_store();
        return ESP_ERR_INVALID_STATE;
    }
    if (app_settings_equal(settings, &s_loaded)) {
        unlock_store();
        return ESP_OK;
    }

    // Keep all keys in one commit. The schema is staged last so a record is
    // never advertised as current before all of its values are staged.
    esp_err_t err = nvs_set_u8(s_handle, KEY_BRIGHTNESS,
                               settings->brightness_percent);
    if (err == ESP_OK) {
        err = nvs_set_u8(s_handle, KEY_PAGE, (uint8_t)settings->selected_page);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(s_handle, KEY_THEME, (uint8_t)settings->theme);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(s_handle, KEY_UNITS, (uint8_t)settings->units);
    }
    if (err == ESP_OK) {
        err = nvs_set_u32(s_handle, KEY_SCHEMA,
                          APP_SETTINGS_SCHEMA_VERSION);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cannot stage settings: %s", esp_err_to_name(err));
        unlock_store();
        return err;
    }

    err = nvs_commit(s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cannot commit settings: %s", esp_err_to_name(err));
        unlock_store();
        return err;
    }

    s_loaded = *settings;
    unlock_store();
    return ESP_OK;
}
