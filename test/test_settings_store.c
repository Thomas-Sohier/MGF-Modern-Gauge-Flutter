#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "infrastructure/settings_store.h"
#include "nvs.h"
#include "freertos/semphr.h"

struct mock_semaphore {
    bool locked;
};

struct mock_nvs_state {
    bool schema_present;
    uint32_t schema;
    uint8_t values[7];
    bool value_present[7];
    uint32_t set_calls;
    uint32_t value_sets_before_schema;
    uint32_t commit_calls;
    esp_err_t flash_error;
    esp_err_t open_error;
    esp_err_t schema_error;
    esp_err_t value_errors[7];
    esp_err_t stage_error;
    esp_err_t commit_error;
    uint32_t staged_schema;
    uint8_t staged_values[7];
};

static struct mock_nvs_state g_nvs;
static struct mock_semaphore g_mutex;

static int value_index(const char *key) {
    static const char *const keys[] = {
        "brightness", "page", "theme", "units", "startup", "utc_q15", "colors",
    };
    for (size_t index = 0; index < sizeof(keys) / sizeof(keys[0]); index++) {
        if (strcmp(key, keys[index]) == 0) return (int)index;
    }
    return -1;
}

static void mock_reset(void) {
    g_nvs = (struct mock_nvs_state){0};
    g_mutex = (struct mock_semaphore){0};
}

const char *esp_err_to_name(esp_err_t error) {
    (void)error;
    return "mock-error";
}

esp_err_t nvs_flash_init(void) {
    return g_nvs.flash_error;
}

esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t open_mode,
                   nvs_handle_t *out_handle) {
    (void)namespace_name;
    (void)open_mode;
    if (g_nvs.open_error != ESP_OK) return g_nvs.open_error;
    *out_handle = 1;
    return ESP_OK;
}

esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key,
                      uint32_t *out_value) {
    (void)handle;
    assert(strcmp(key, "schema") == 0);
    if (g_nvs.schema_error != ESP_OK) return g_nvs.schema_error;
    if (!g_nvs.schema_present) return ESP_ERR_NVS_NOT_FOUND;
    *out_value = g_nvs.schema;
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *out_value) {
    (void)handle;
    const int index = value_index(key);
    assert(index >= 0);
    if (g_nvs.value_errors[index] != ESP_OK) return g_nvs.value_errors[index];
    if (!g_nvs.value_present[index]) return ESP_ERR_NVS_NOT_FOUND;
    *out_value = g_nvs.values[index];
    return ESP_OK;
}

esp_err_t nvs_set_u32(nvs_handle_t handle, const char *key, uint32_t value) {
    (void)handle;
    assert(strcmp(key, "schema") == 0);
    if (g_nvs.stage_error != ESP_OK) return g_nvs.stage_error;
    g_nvs.set_calls++;
    g_nvs.staged_schema = value;
    assert(g_nvs.value_sets_before_schema == 7U);
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value) {
    (void)handle;
    const int index = value_index(key);
    assert(index >= 0);
    if (g_nvs.stage_error != ESP_OK) return g_nvs.stage_error;
    if (g_nvs.value_sets_before_schema == 7U) {
        g_nvs.value_sets_before_schema = 0;
    }
    g_nvs.set_calls++;
    g_nvs.value_sets_before_schema++;
    g_nvs.staged_values[index] = value;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle) {
    (void)handle;
    g_nvs.commit_calls++;
    if (g_nvs.commit_error != ESP_OK) return g_nvs.commit_error;
    g_nvs.schema = g_nvs.staged_schema;
    g_nvs.schema_present = true;
    memcpy(g_nvs.values, g_nvs.staged_values, sizeof(g_nvs.values));
    memset(g_nvs.value_present, true, sizeof(g_nvs.value_present));
    return ESP_OK;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return &g_mutex;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout) {
    (void)timeout;
    assert(semaphore != NULL && !semaphore->locked);
    semaphore->locked = true;
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) {
    assert(semaphore != NULL && semaphore->locked);
    semaphore->locked = false;
    return pdTRUE;
}

static void test_defaults_and_noop_save(void) {
    app_settings_t defaults;
    app_settings_defaults(&defaults);

    app_settings_t out = {0};
    assert(settings_store_load(&out) == ESP_ERR_INVALID_STATE);
    assert(settings_store_save(&defaults) == ESP_ERR_INVALID_STATE);
    assert(settings_store_init() == ESP_OK);
    assert(settings_store_load(&out) == ESP_OK);
    assert(app_settings_equal(&out, &defaults));
    assert(settings_store_save(&defaults) == ESP_OK);
    assert(g_nvs.set_calls == 0U);
    assert(g_nvs.commit_calls == 0U);
}

static void test_transaction_and_reload(void) {
    app_settings_t changed;
    app_settings_defaults(&changed);
    changed.brightness_percent = 42;

    assert(settings_store_save(&changed) == ESP_OK);
    assert(g_nvs.set_calls == 8U);
    assert(g_nvs.commit_calls == 1U);
    assert(g_nvs.schema == APP_SETTINGS_SCHEMA_VERSION);
    assert(g_nvs.values[0] == 42U);
    assert(settings_store_save(&changed) == ESP_OK);
    assert(g_nvs.set_calls == 8U);
    assert(g_nvs.commit_calls == 1U);

    app_settings_t out = {0};
    assert(settings_store_load(&out) == ESP_OK);
    assert(app_settings_equal(&out, &changed));
}

static void test_invalid_records_use_defaults(void) {
    app_settings_t defaults;
    app_settings_defaults(&defaults);
    app_settings_t out = {0};

    g_nvs.values[1] = APP_SETTINGS_PAGE_COUNT;
    assert(settings_store_load(&out) == ESP_OK);
    assert(app_settings_equal(&out, &defaults));

    g_nvs.values[1] = APP_SETTINGS_PAGE_RPM;
    g_nvs.value_errors[1] = ESP_ERR_NVS_TYPE_MISMATCH;
    assert(settings_store_load(&out) == ESP_OK);
    assert(app_settings_equal(&out, &defaults));

    g_nvs.value_errors[1] = ESP_OK;
    g_nvs.schema_error = ESP_ERR_NVS_TYPE_MISMATCH;
    assert(settings_store_load(&out) == ESP_OK);
    assert(app_settings_equal(&out, &defaults));
    g_nvs.schema_error = ESP_OK;

    g_nvs.schema = APP_SETTINGS_SCHEMA_VERSION + 1U;
    assert(settings_store_load(&out) == ESP_OK);
    assert(app_settings_equal(&out, &defaults));
    g_nvs.schema = APP_SETTINGS_SCHEMA_VERSION;
}

static void test_unexpected_errors_disable_save_until_reload(void) {
    app_settings_t changed;
    app_settings_defaults(&changed);
    changed.units = APP_SETTINGS_UNITS_IMPERIAL;
    app_settings_t out = {0};

    g_nvs.schema_error = ESP_ERR_MOCK_IO;
    assert(settings_store_load(&out) == ESP_ERR_MOCK_IO);
    assert(app_settings_is_valid(&out));
    assert(settings_store_save(&changed) == ESP_ERR_INVALID_STATE);

    g_nvs.schema_error = ESP_OK;
    assert(settings_store_load(&out) == ESP_OK);
    g_nvs.commit_error = ESP_ERR_MOCK_IO;
    assert(settings_store_save(&changed) == ESP_ERR_MOCK_IO);
    g_nvs.commit_error = ESP_OK;
    assert(settings_store_save(&changed) == ESP_OK);
    assert(g_nvs.commit_calls == 3U);
}

static void test_schema_1_is_migrated(void) {
    // Enregistrement schéma 1 : pas de clé « startup ».
    g_nvs.schema = 1U;
    g_nvs.values[0] = 60U;
    g_nvs.values[1] = APP_SETTINGS_PAGE_TEMPERATURES;
    g_nvs.values[2] = APP_SETTINGS_THEME_AMBER;
    g_nvs.values[3] = APP_SETTINGS_UNITS_IMPERIAL;
    g_nvs.value_present[4] = false;
    g_nvs.value_errors[4] = ESP_OK;
    g_nvs.value_present[5] = false;

    app_settings_t out = {0};
    assert(settings_store_load(&out) == ESP_OK);
    assert(out.brightness_percent == 60U);
    assert(out.selected_page == APP_SETTINGS_PAGE_TEMPERATURES);
    assert(out.units == APP_SETTINGS_UNITS_IMPERIAL);
    assert(out.startup_page == APP_SETTINGS_STARTUP_LAST_PAGE);
    assert(app_settings_boot_page(&out) == APP_SETTINGS_PAGE_TEMPERATURES);

    // Mêmes valeurs, mais la flash est encore au schéma 1 : réécriture.
    const uint32_t commits = g_nvs.commit_calls;
    assert(settings_store_save(&out) == ESP_OK);
    assert(g_nvs.commit_calls == commits + 1U);
    assert(g_nvs.schema == APP_SETTINGS_SCHEMA_VERSION);
    assert(g_nvs.values[4] == (uint8_t)APP_SETTINGS_STARTUP_LAST_PAGE);
    assert(g_nvs.values[5] == 64U); // UTC+0
    assert(settings_store_save(&out) == ESP_OK);
    assert(g_nvs.commit_calls == commits + 1U);

    // Schéma 2 : la page de démarrage fixe est relue.
    out.startup_page = APP_SETTINGS_PAGE_RPM;
    out.utc_offset_minutes = -570; // UTC-09:30
    assert(settings_store_save(&out) == ESP_OK);
    app_settings_t reloaded = {0};
    assert(settings_store_load(&reloaded) == ESP_OK);
    assert(app_settings_equal(&reloaded, &out));
    assert(app_settings_boot_page(&reloaded) == APP_SETTINGS_PAGE_RPM);
}

static void test_schema_2_is_migrated_to_normal_colors(void) {
    // Enregistrement schéma 2 : startup + fuseau, pas de clé « colors ».
    g_nvs.schema = 2U;
    g_nvs.values[0] = 35U;
    g_nvs.values[1] = APP_SETTINGS_PAGE_MUSIC;
    g_nvs.values[2] = APP_SETTINGS_THEME_AMBER;
    g_nvs.values[3] = APP_SETTINGS_UNITS_IMPERIAL;
    g_nvs.values[4] = APP_SETTINGS_PAGE_CLOCK;
    g_nvs.values[5] = 64U + 8U; // UTC+2
    g_nvs.value_present[6] = false;
    // Une valeur résiduelle ne doit pas être lue pour un schéma 2.
    g_nvs.values[6] = APP_SETTINGS_COLORS_INVERTED;

    app_settings_t out = {0};
    assert(settings_store_load(&out) == ESP_OK);
    assert(out.brightness_percent == 35U);
    assert(out.selected_page == APP_SETTINGS_PAGE_MUSIC);
    assert(out.units == APP_SETTINGS_UNITS_IMPERIAL);
    assert(out.startup_page == APP_SETTINGS_PAGE_CLOCK);
    assert(out.utc_offset_minutes == 120);
    assert(out.color_mode == APP_SETTINGS_COLORS_NORMAL);

    // Valeurs inchangées mais flash au schéma 2 : une seule réécriture v3.
    const uint32_t commits = g_nvs.commit_calls;
    assert(settings_store_save(&out) == ESP_OK);
    assert(g_nvs.commit_calls == commits + 1U);
    assert(g_nvs.schema == APP_SETTINGS_SCHEMA_VERSION);
    assert(g_nvs.values[6] == (uint8_t)APP_SETTINGS_COLORS_NORMAL);
    assert(g_nvs.values[4] == (uint8_t)APP_SETTINGS_PAGE_CLOCK);
    assert(g_nvs.values[5] == 72U);
    assert(settings_store_save(&out) == ESP_OK);
    assert(g_nvs.commit_calls == commits + 1U);
}

static void test_inverted_colors_round_trip(void) {
    app_settings_t out = {0};
    assert(settings_store_load(&out) == ESP_OK);
    out.color_mode = APP_SETTINGS_COLORS_INVERTED;
    assert(settings_store_save(&out) == ESP_OK);
    assert(g_nvs.values[6] == (uint8_t)APP_SETTINGS_COLORS_INVERTED);

    app_settings_t reloaded = {0};
    assert(settings_store_load(&reloaded) == ESP_OK);
    assert(reloaded.color_mode == APP_SETTINGS_COLORS_INVERTED);
    assert(app_settings_equal(&reloaded, &out));

    // Palette inconnue : enregistrement rejeté, retour aux défauts.
    app_settings_t defaults;
    app_settings_defaults(&defaults);
    g_nvs.values[6] = APP_SETTINGS_COLORS_COUNT;
    assert(settings_store_load(&reloaded) == ESP_OK);
    assert(app_settings_equal(&reloaded, &defaults));

    // Schéma 3 sans clé « colors » : incomplet, défauts (jamais deviné).
    g_nvs.values[6] = APP_SETTINGS_COLORS_INVERTED;
    g_nvs.value_present[6] = false;
    assert(settings_store_load(&reloaded) == ESP_OK);
    assert(app_settings_equal(&reloaded, &defaults));
    g_nvs.value_present[6] = true;
}

static void test_invalid_save_values_are_rejected(void) {
    app_settings_t settings;
    app_settings_defaults(&settings);
    settings.selected_page = (app_settings_page_t)-1;
    assert(settings_store_save(&settings) == ESP_ERR_INVALID_ARG);
}

int main(void) {
    mock_reset();
    test_defaults_and_noop_save();
    test_transaction_and_reload();
    test_invalid_records_use_defaults();
    test_unexpected_errors_disable_save_until_reload();
    test_schema_1_is_migrated();
    test_schema_2_is_migrated_to_normal_colors();
    test_inverted_colors_round_trip();
    test_invalid_save_values_are_rejected();
    puts("settings store tests: OK");
    return 0;
}
