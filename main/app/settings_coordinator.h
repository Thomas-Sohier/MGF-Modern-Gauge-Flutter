#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "domain/app_settings.h"

#define SETTINGS_COORDINATOR_DEBOUNCE_MS 3000U

typedef struct settings_coordinator_s settings_coordinator_t;

// Return true only after the complete snapshot has been persisted.
typedef bool (*settings_coordinator_save_cb_t)(
    void *context, const app_settings_t *settings);

settings_coordinator_t *settings_coordinator_create(
    const app_settings_t *initial, settings_coordinator_save_cb_t save,
    void *save_context);
void settings_coordinator_destroy(settings_coordinator_t *coordinator);

const app_settings_t *settings_coordinator_current(
    const settings_coordinator_t *coordinator);
bool settings_coordinator_is_dirty(
    const settings_coordinator_t *coordinator);

// These typed setters are the boundary future controls should use. They do
// not write storage; tick() performs the debounced save.
bool settings_coordinator_set_brightness(settings_coordinator_t *coordinator,
                                          uint8_t brightness_percent);
bool settings_coordinator_set_page(settings_coordinator_t *coordinator,
                                   app_settings_page_t page);
bool settings_coordinator_set_theme(settings_coordinator_t *coordinator,
                                    app_settings_theme_t theme);
bool settings_coordinator_set_units(settings_coordinator_t *coordinator,
                                    app_settings_units_t units);

// Applies a complete validated snapshot, useful for future settings screens.
bool settings_coordinator_update(settings_coordinator_t *coordinator,
                                 const app_settings_t *settings);

// This function is suitable for dashboard_navigator_set_page_changed_callback.
void settings_coordinator_page_changed(void *context, size_t page_index);

// Call from an application timer. Changes made since the previous call start
// (or restart) the quiet-period timer. A failed save remains dirty and is
// retried after the same interval.
bool settings_coordinator_tick(settings_coordinator_t *coordinator,
                               uint32_t now_ms);
