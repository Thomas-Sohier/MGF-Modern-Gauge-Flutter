#include "infrastructure/settings_store.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "settings";

#define SETTINGS_NAMESPACE "app_settings"
#define KEY_SCHEMA "schema"
#define KEY_BRIGHTNESS "brightness"
#define KEY_PAGE "page"
#define KEY_THEME "theme"
#define KEY_UNITS "units"

static nvs_handle_t s_handle;
static app_settings_t s_loaded;
static bool s_initialized;
static bool s_has_loaded;

static void use_defaults(app_settings_t *out, const char *reason) {
    app_settings_defaults(out);
    ESP_LOGW(TAG, "using default settings (%s)", reason);
}

static bool read_u8(nvs_handle_t handle, const char *key, uint8_t *value) {
    return nvs_get_u8(handle, key, value) == ESP_OK;
}

esp_err_t settings_store_init(void) {
    if (s_initialized) return ESP_OK;

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
    if (!s_initialized) return ESP_ERR_INVALID_STATE;

    app_settings_defaults(out);

    uint32_t schema = 0;
    esp_err_t err = nvs_get_u32(s_handle, KEY_SCHEMA, &schema);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        use_defaults(out, "first boot");
        s_loaded = *out;
        s_has_loaded = true;
        return ESP_OK;
    }
    if (err != ESP_OK) {
        use_defaults(out, "schema is unreadable");
        s_loaded = *out;
        s_has_loaded = true;
        return ESP_OK;
    }
    if (schema != APP_SETTINGS_SCHEMA_VERSION) {
        use_defaults(out, "unsupported schema");
        s_loaded = *out;
        s_has_loaded = true;
        return ESP_OK;
    }

    uint8_t brightness = 0;
    uint8_t page = 0;
    uint8_t theme = 0;
    uint8_t units = 0;
    if (!read_u8(s_handle, KEY_BRIGHTNESS, &brightness) ||
        !read_u8(s_handle, KEY_PAGE, &page) ||
        !read_u8(s_handle, KEY_THEME, &theme) ||
        !read_u8(s_handle, KEY_UNITS, &units)) {
        use_defaults(out, "settings are incomplete");
    } else {
        *out = (app_settings_t){
            .brightness_percent = brightness,
            .selected_page = (app_settings_page_t)page,
            .theme = (app_settings_theme_t)theme,
            .units = (app_settings_units_t)units,
        };
        if (!app_settings_is_valid(out)) {
            use_defaults(out, "settings are invalid");
        }
    }

    s_loaded = *out;
    s_has_loaded = true;
    return ESP_OK;
}

esp_err_t settings_store_save(const app_settings_t *settings) {
    if (settings == NULL || !app_settings_is_valid(settings)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || !s_has_loaded) return ESP_ERR_INVALID_STATE;
    if (app_settings_equal(settings, &s_loaded)) return ESP_OK;

    esp_err_t err = nvs_set_u32(s_handle, KEY_SCHEMA,
                                APP_SETTINGS_SCHEMA_VERSION);
    if (err == ESP_OK) {
        err = nvs_set_u8(s_handle, KEY_BRIGHTNESS,
                         settings->brightness_percent);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(s_handle, KEY_PAGE, (uint8_t)settings->selected_page);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(s_handle, KEY_THEME, (uint8_t)settings->theme);
    }
    if (err == ESP_OK) {
        err = nvs_set_u8(s_handle, KEY_UNITS, (uint8_t)settings->units);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cannot stage settings: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_commit(s_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cannot commit settings: %s", esp_err_to_name(err));
        return err;
    }

    s_loaded = *settings;
    return ESP_OK;
}
