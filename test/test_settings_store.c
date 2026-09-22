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
    uint8_t values[4];
    bool value_present[4];
    uint32_t set_calls;
    uint32_t value_sets_before_schema;
    uint32_t commit_calls;
    esp_err_t flash_error;
    esp_err_t open_error;
    esp_err_t schema_error;
    esp_err_t value_errors[4];
    esp_err_t stage_error;
    esp_err_t commit_error;
    uint32_t staged_schema;
    uint8_t staged_values[4];
};

static struct mock_nvs_state g_nvs;
static struct mock_semaphore g_mutex;

static int value_index(const char *key) {
    static const char *const keys[] = {
        "brightness", "page", "theme", "units",
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

esp_err_t nvs_get_u32(nvs_handle_t handle, const char *key, uint32_t *out_value) {
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
    assert(g_nvs.value_sets_before_schema == 4U);
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value) {
    (void)handle;
    const int index = value_index(key);
    assert(index >= 0);
    if (g_nvs.stage_error != ESP_OK) return g_nvs.stage_error;
    if (g_nvs.value_sets_before_schema == 4U) {
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
    assert(g_nvs.set_calls == 5U);
    assert(g_nvs.commit_calls == 1U);
    assert(g_nvs.schema == APP_SETTINGS_SCHEMA_VERSION);
    assert(g_nvs.values[0] == 42U);
    assert(settings_store_save(&changed) == ESP_OK);
    assert(g_nvs.set_calls == 5U);
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
    test_invalid_save_values_are_rejected();
    puts("settings store tests: OK");
    return 0;
}
