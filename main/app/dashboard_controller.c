#include "app/dashboard_controller.h"

#include "lvgl.h"

struct dashboard_controller_s {
    dashboard_navigator_t *navigator;
    ecu_source_t ecu_source;
    uint32_t period_ms;
    ecu_data_t snapshot;
    lv_timer_t *timer;
};

// lv_timer callbacks are dispatched by LVGL's own thread. The caller holds
// the LVGL lock only while creating/starting the timer; taking it again here
// would deadlock when the platform lock is non-recursive.
static void dashboard_controller_tick(lv_timer_t *timer) {
    dashboard_controller_t *controller = lv_timer_get_user_data(timer);
    if (controller == NULL) return;

    ecu_data_t next;
    if (!ecu_source_read(&controller->ecu_source, &next)) {
        // Keep the last valid snapshot on transport/source errors. In
        // particular, do not update or destroy the screen in this case.
        return;
    }

    controller->snapshot = next;
    dashboard_navigator_update(controller->navigator, &controller->snapshot);
}

dashboard_controller_t *
dashboard_controller_create(const dashboard_controller_config_t *config) {
    if (config == NULL || config->navigator == NULL ||
        config->ecu_source.read == NULL || config->period_ms == 0) {
        return NULL;
    }

    dashboard_controller_t *controller = lv_malloc(sizeof(*controller));
    if (controller == NULL) return NULL;

    controller->navigator = config->navigator;
    controller->ecu_source = config->ecu_source;
    controller->period_ms = config->period_ms;
    controller->snapshot = (ecu_data_t){0};
    controller->timer = NULL;
    return controller;
}

bool dashboard_controller_start(dashboard_controller_t *controller) {
    if (controller == NULL) return false;
    if (controller->timer != NULL) return true;

    controller->timer = lv_timer_create(dashboard_controller_tick,
                                        controller->period_ms, controller);
    return controller->timer != NULL;
}

void dashboard_controller_destroy(dashboard_controller_t *controller) {
    if (controller == NULL) return;

    if (controller->timer != NULL) lv_timer_delete(controller->timer);
    lv_free(controller);
}
