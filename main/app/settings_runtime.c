#include "app/settings_runtime.h"

#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct settings_runtime_s {
    settings_coordinator_t *coordinator;
    SemaphoreHandle_t mutex;
    app_settings_t published;
    app_settings_t pending;
    bool has_pending;
    settings_runtime_apply_cb_t apply;
    void *apply_context;
};

static bool lock(settings_runtime_t *runtime) {
    return runtime != NULL && runtime->mutex != NULL &&
           xSemaphoreTake(runtime->mutex, portMAX_DELAY) == pdTRUE;
}

static void unlock(settings_runtime_t *runtime) {
    xSemaphoreGive(runtime->mutex);
}

static void publish(settings_runtime_t *runtime) {
    runtime->published = *settings_coordinator_current(runtime->coordinator);
}

static bool apply_current(settings_runtime_t *runtime) {
    if (runtime == NULL || runtime->apply == NULL) return false;
    const app_settings_t *settings =
        settings_coordinator_current(runtime->coordinator);
    return settings != NULL && runtime->apply(runtime->apply_context, settings);
}

static bool apply_update(settings_runtime_t *runtime,
                         const app_settings_t *settings) {
    if (runtime == NULL || settings == NULL ||
        !settings_coordinator_update(runtime->coordinator, settings)) {
        return false;
    }
    if (!apply_current(runtime)) return false;

    if (!lock(runtime)) return false;
    publish(runtime);
    unlock(runtime);
    return true;
}

settings_runtime_t *settings_runtime_create(const app_settings_t *initial,
                                            settings_coordinator_save_cb_t save,
                                            void *save_context,
                                            settings_runtime_apply_cb_t apply,
                                            void *apply_context) {
    if (initial == NULL || save == NULL || apply == NULL ||
        !app_settings_is_valid(initial)) {
        return NULL;
    }

    settings_runtime_t *runtime = calloc(1, sizeof(*runtime));
    if (runtime == NULL) return NULL;
    runtime->mutex = xSemaphoreCreateMutex();
    if (runtime->mutex == NULL) {
        free(runtime);
        return NULL;
    }

    runtime->coordinator =
        settings_coordinator_create(initial, save, save_context);
    if (runtime->coordinator == NULL) {
        vSemaphoreDelete(runtime->mutex);
        free(runtime);
        return NULL;
    }
    runtime->published = *initial;
    runtime->apply = apply;
    runtime->apply_context = apply_context;
    return runtime;
}

void settings_runtime_destroy(settings_runtime_t *runtime) {
    if (runtime == NULL) return;
    settings_coordinator_destroy(runtime->coordinator);
    if (runtime->mutex != NULL) vSemaphoreDelete(runtime->mutex);
    free(runtime);
}

bool settings_runtime_read(void *context, app_settings_t *out) {
    settings_runtime_t *runtime = context;
    if (out == NULL || !lock(runtime)) return false;
    *out = runtime->published;
    const bool valid = app_settings_is_valid(out);
    unlock(runtime);
    return valid;
}

bool settings_runtime_submit(void *context, const app_settings_t *settings) {
    settings_runtime_t *runtime = context;
    if (settings == NULL || !app_settings_is_valid(settings) ||
        !lock(runtime)) {
        return false;
    }
    runtime->pending = *settings;
    runtime->has_pending = true;
    unlock(runtime);
    return true;
}

bool settings_runtime_set_brightness(settings_runtime_t *runtime,
                                     uint8_t brightness_percent) {
    if (runtime == NULL || brightness_percent > 100U) return false;
    app_settings_t next = *settings_coordinator_current(runtime->coordinator);
    next.brightness_percent = brightness_percent;
    return apply_update(runtime, &next);
}

bool settings_runtime_set_page(settings_runtime_t *runtime,
                               app_settings_page_t page) {
    if (runtime == NULL || page >= APP_SETTINGS_PAGE_COUNT) return false;
    app_settings_t next = *settings_coordinator_current(runtime->coordinator);
    next.selected_page = page;
    return apply_update(runtime, &next);
}

bool settings_runtime_set_startup_page(settings_runtime_t *runtime,
                                       app_settings_page_t page) {
    if (runtime == NULL) return false;
    app_settings_t next = *settings_coordinator_current(runtime->coordinator);
    next.startup_page = page;
    if (!app_settings_is_valid(&next)) return false;
    return apply_update(runtime, &next);
}

bool settings_runtime_set_utc_offset(settings_runtime_t *runtime,
                                     int16_t utc_offset_minutes) {
    if (runtime == NULL) return false;
    app_settings_t next = *settings_coordinator_current(runtime->coordinator);
    next.utc_offset_minutes = utc_offset_minutes;
    if (!app_settings_is_valid(&next)) return false;
    return apply_update(runtime, &next);
}

bool settings_runtime_set_color_mode(settings_runtime_t *runtime,
                                     app_settings_color_mode_t color_mode) {
    if (runtime == NULL) return false;
    app_settings_t next = *settings_coordinator_current(runtime->coordinator);
    next.color_mode = color_mode;
    if (!app_settings_is_valid(&next)) return false;
    return apply_update(runtime, &next);
}

bool settings_runtime_update(settings_runtime_t *runtime,
                             const app_settings_t *settings) {
    if (runtime == NULL || settings == NULL || !app_settings_is_valid(settings))
        return false;
    return apply_update(runtime, settings);
}

void settings_runtime_page_changed(void *context, size_t page_index) {
    settings_runtime_t *runtime = context;
    if (runtime == NULL || page_index >= APP_SETTINGS_PAGE_COUNT) return;
    if (!settings_coordinator_set_page(runtime->coordinator,
                                       (app_settings_page_t)page_index)) {
        return;
    }
    if (!lock(runtime)) return;
    publish(runtime);
    unlock(runtime);
}

bool settings_runtime_apply_current(settings_runtime_t *runtime) {
    if (runtime == NULL || !apply_current(runtime)) return false;
    if (!lock(runtime)) return false;
    publish(runtime);
    unlock(runtime);
    return true;
}

bool settings_runtime_process(settings_runtime_t *runtime, uint32_t now_ms) {
    if (runtime == NULL) return false;

    app_settings_t pending;
    bool has_pending = false;
    if (lock(runtime)) {
        if (runtime->has_pending) {
            pending = runtime->pending;
            runtime->has_pending = false;
            has_pending = true;
        }
        unlock(runtime);
    }

    if (has_pending && !apply_update(runtime, &pending)) {
        // Keep the request for a later LVGL tick if applying it failed. The
        // latest request wins, which also bounds memory and flash pressure.
        settings_runtime_submit(runtime, &pending);
    }
    return settings_coordinator_tick(runtime->coordinator, now_ms);
}
