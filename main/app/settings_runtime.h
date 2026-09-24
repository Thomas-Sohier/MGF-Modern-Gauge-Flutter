#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/settings_coordinator.h"

// Bridges the NimBLE task and the LVGL task. BLE callbacks only copy a
// validated snapshot into the pending slot; settings_runtime_process() applies
// it, updates widgets, and performs the debounced NVS write in the LVGL task.
typedef struct settings_runtime_s settings_runtime_t;

typedef bool (*settings_runtime_apply_cb_t)(
    void *context, const app_settings_t *settings);

settings_runtime_t *settings_runtime_create(
    const app_settings_t *initial, settings_coordinator_save_cb_t save,
    void *save_context, settings_runtime_apply_cb_t apply,
    void *apply_context);
void settings_runtime_destroy(settings_runtime_t *runtime);

// These callbacks are safe to pass to the NimBLE configuration service. They
// never touch LVGL or NVS and copy through a short-lived mutex-protected slot.
bool settings_runtime_read(void *context, app_settings_t *out);
bool settings_runtime_submit(void *context, const app_settings_t *settings);

// The following functions run in the LVGL task. They apply the setting
// immediately; persistence remains debounced by settings_runtime_process().
bool settings_runtime_set_brightness(settings_runtime_t *runtime,
                                      uint8_t brightness_percent);
bool settings_runtime_set_page(settings_runtime_t *runtime,
                               app_settings_page_t page);
bool settings_runtime_set_startup_page(settings_runtime_t *runtime,
                                       app_settings_page_t page);
// Décalage UTC -> heure légale reçu du téléphone (persisté, anti-rebond).
bool settings_runtime_set_utc_offset(settings_runtime_t *runtime,
                                     int16_t utc_offset_minutes);
bool settings_runtime_set_theme(settings_runtime_t *runtime,
                                app_settings_theme_t theme);
bool settings_runtime_set_units(settings_runtime_t *runtime,
                                app_settings_units_t units);
bool settings_runtime_update(settings_runtime_t *runtime,
                             const app_settings_t *settings);

// Suitable for dashboard_navigator_set_page_changed_callback(). The
// navigator has already changed page when this callback runs.
void settings_runtime_page_changed(void *context, size_t page_index);

// Applies the currently published snapshot once during startup.
bool settings_runtime_apply_current(settings_runtime_t *runtime);

// Call from an LVGL timer. Returns true when a snapshot was persisted.
bool settings_runtime_process(settings_runtime_t *runtime, uint32_t now_ms);
