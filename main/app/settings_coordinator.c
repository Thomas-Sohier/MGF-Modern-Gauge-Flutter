#include "app/settings_coordinator.h"

#include <stdlib.h>

struct settings_coordinator_s {
    app_settings_t current;
    app_settings_t persisted;
    settings_coordinator_save_cb_t save;
    void *save_context;
    uint32_t change_sequence;
    uint32_t scheduled_sequence;
    uint32_t due_ms;
    bool dirty;
};

static bool time_reached(uint32_t now_ms, uint32_t due_ms) {
    return (int32_t)(now_ms - due_ms) >= 0;
}

static void mark_changed(settings_coordinator_t *coordinator) {
    coordinator->dirty = true;
    coordinator->change_sequence++;
}

settings_coordinator_t *settings_coordinator_create(
    const app_settings_t *initial, settings_coordinator_save_cb_t save,
    void *save_context) {
    if (initial == NULL || save == NULL || !app_settings_is_valid(initial)) {
        return NULL;
    }

    settings_coordinator_t *coordinator = calloc(1, sizeof(*coordinator));
    if (coordinator == NULL) return NULL;

    coordinator->current = *initial;
    coordinator->persisted = *initial;
    coordinator->save = save;
    coordinator->save_context = save_context;
    return coordinator;
}

void settings_coordinator_destroy(settings_coordinator_t *coordinator) {
    free(coordinator);
}

const app_settings_t *settings_coordinator_current(
    const settings_coordinator_t *coordinator) {
    return coordinator == NULL ? NULL : &coordinator->current;
}

bool settings_coordinator_is_dirty(
    const settings_coordinator_t *coordinator) {
    return coordinator != NULL && coordinator->dirty;
}

bool settings_coordinator_update(settings_coordinator_t *coordinator,
                                 const app_settings_t *settings) {
    if (coordinator == NULL || settings == NULL ||
        !app_settings_is_valid(settings)) {
        return false;
    }
    if (app_settings_equal(settings, &coordinator->current)) return true;

    coordinator->current = *settings;
    if (app_settings_equal(&coordinator->current, &coordinator->persisted)) {
        coordinator->dirty = false;
    } else {
        mark_changed(coordinator);
    }
    return true;
}

bool settings_coordinator_set_brightness(settings_coordinator_t *coordinator,
                                          uint8_t brightness_percent) {
    if (coordinator == NULL || brightness_percent > 100U) return false;
    app_settings_t next = coordinator->current;
    next.brightness_percent = brightness_percent;
    return settings_coordinator_update(coordinator, &next);
}

bool settings_coordinator_set_page(settings_coordinator_t *coordinator,
                                   app_settings_page_t page) {
    if (coordinator == NULL || page >= APP_SETTINGS_PAGE_COUNT) return false;
    app_settings_t next = coordinator->current;
    next.selected_page = page;
    return settings_coordinator_update(coordinator, &next);
}

bool settings_coordinator_set_startup_page(settings_coordinator_t *coordinator,
                                           app_settings_page_t page) {
    if (coordinator == NULL) return false;
    app_settings_t next = coordinator->current;
    next.startup_page = page;
    return settings_coordinator_update(coordinator, &next);
}

bool settings_coordinator_set_theme(settings_coordinator_t *coordinator,
                                    app_settings_theme_t theme) {
    if (coordinator == NULL || theme >= APP_SETTINGS_THEME_COUNT) return false;
    app_settings_t next = coordinator->current;
    next.theme = theme;
    return settings_coordinator_update(coordinator, &next);
}

bool settings_coordinator_set_units(settings_coordinator_t *coordinator,
                                    app_settings_units_t units) {
    if (coordinator == NULL || units >= APP_SETTINGS_UNITS_COUNT) return false;
    app_settings_t next = coordinator->current;
    next.units = units;
    return settings_coordinator_update(coordinator, &next);
}

void settings_coordinator_page_changed(void *context, size_t page_index) {
    if (page_index >= APP_SETTINGS_PAGE_COUNT) return;
    settings_coordinator_set_page(context, (app_settings_page_t)page_index);
}

bool settings_coordinator_tick(settings_coordinator_t *coordinator,
                               uint32_t now_ms) {
    if (coordinator == NULL || !coordinator->dirty) return false;

    if (coordinator->change_sequence != coordinator->scheduled_sequence) {
        coordinator->due_ms = now_ms + SETTINGS_COORDINATOR_DEBOUNCE_MS;
        coordinator->scheduled_sequence = coordinator->change_sequence;
        return false;
    }
    if (!time_reached(now_ms, coordinator->due_ms)) return false;

    if (!coordinator->save(coordinator->save_context, &coordinator->current)) {
        coordinator->due_ms = now_ms + SETTINGS_COORDINATOR_DEBOUNCE_MS;
        return false;
    }

    coordinator->persisted = coordinator->current;
    coordinator->dirty = false;
    return true;
}
