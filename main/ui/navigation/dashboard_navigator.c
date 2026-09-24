#include "ui/navigation/dashboard_navigator.h"
#include "ui/navigation/dashboard_timing.h"
#include "ui/navigation/ui_instrumentation.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"

#define DASHBOARD_MAX_PAGES 12
#define GESTURE_CLICK_SUPPRESSION_MS 300U
// Maintien volontairement plus long que l'appui long LVGL (400 ms) : un
// effleurement en roulant ne doit pas ouvrir les réglages.
#define HOLD_ACTION_MS 1000U

// Indicateur de position : un point par page dans l'ouverture basse du cadran,
// point courant éclairci. Visible dès qu'il y a plus d'une page à parcourir.
#define INDICATOR_CX       160.0f
#define INDICATOR_Y        298.0f
#define INDICATOR_SPACING  11.0f
#define INDICATOR_R          2.6f
#define INDICATOR_R_CURRENT  3.6f

typedef struct {
    lv_obj_t *object;
    dashboard_page_t descriptor;
    uint32_t last_update_ms;
    bool has_presented_data;
} page_entry_t;

struct dashboard_navigator_s {
    lv_obj_t *root;
    lv_obj_t *indicator;
    page_entry_t pages[DASHBOARD_MAX_PAGES];
    size_t count;
    size_t current;
    ecu_data_t latest_data;
    bool has_latest_data;
    uint32_t last_gesture_ms;
    dashboard_page_changed_cb_t page_changed;
    void *page_changed_context;
    dashboard_hold_cb_t hold;
    void *hold_context;
    uint32_t press_started_ms;
    bool press_is_gesture;
    bool press_held;
};

static void update_current(dashboard_navigator_t *navigator, bool force) {
    if (!navigator->has_latest_data || navigator->count == 0) return;

    page_entry_t *page = &navigator->pages[navigator->current];
    const uint32_t period = page->descriptor.update_period_ms;
    if (!force && (period == 0 ||
                   (page->has_presented_data &&
                    !dashboard_period_elapsed(lv_tick_get(),
                                              page->last_update_ms, period)))) {
        return;
    }

    page->last_update_ms = lv_tick_get();
    page->has_presented_data = true;
    if (page->descriptor.update != NULL) {
        const uint64_t started_at = ui_instrumentation_begin();
        page->descriptor.update(page->descriptor.context,
                                &navigator->latest_data);
        ui_instrumentation_end(page->descriptor.name, started_at);
    }
}

static void show_current(dashboard_navigator_t *navigator) {
    for (size_t i = 0; i < navigator->count; i++) {
        if (i == navigator->current) lv_obj_remove_flag(navigator->pages[i].object, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(navigator->pages[i].object, LV_OBJ_FLAG_HIDDEN);
    }
    // La pile de pages est au-dessus de l'indicateur : le remettre au premier
    // plan après toute bascule, puis redessiner le point courant.
    if (navigator->indicator != NULL) {
        lv_obj_move_foreground(navigator->indicator);
        lv_obj_invalidate(navigator->indicator);
    }
}

static void indicator_draw_cb(lv_event_t *event) {
    lv_obj_t *object = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    dashboard_navigator_t *navigator = lv_obj_get_user_data(object);
    if (navigator == NULL || navigator->count < 2) return;

    lv_area_t area;
    lv_obj_get_coords(object, &area);
    const ui_layout_t layout = amber_draw_layout(&area);
    const float span = INDICATOR_SPACING * (float)(navigator->count - 1);
    const float x0 = INDICATOR_CX - span * 0.5f;
    for (size_t i = 0; i < navigator->count; i++) {
        const bool current = i == navigator->current;
        amber_draw_dot(layer, &layout, x0 + INDICATOR_SPACING * (float)i,
                       INDICATOR_Y, current ? INDICATOR_R_CURRENT
                                            : INDICATOR_R,
                       current ? ui_theme_amber_bright()
                               : ui_theme_amber_dim());
    }
}

bool dashboard_navigator_select_page(dashboard_navigator_t *navigator,
                                      size_t page_index) {
    if (navigator == NULL || page_index >= navigator->count) return false;
    if (page_index == navigator->current) return true;

    navigator->current = page_index;
    show_current(navigator);
    update_current(navigator, true);
    if (navigator->page_changed != NULL) {
        navigator->page_changed(navigator->page_changed_context, page_index);
    }
    return true;
}

void dashboard_navigator_next(dashboard_navigator_t *navigator) {
    if (navigator == NULL || navigator->count < 2) return;
    dashboard_navigator_select_page(navigator,
                                    (navigator->current + 1) % navigator->count);
}

void dashboard_navigator_previous(dashboard_navigator_t *navigator) {
    if (navigator == NULL || navigator->count < 2) return;
    dashboard_navigator_select_page(
        navigator, (navigator->current + navigator->count - 1) % navigator->count);
}

static bool point_in_visible_disc(const dashboard_navigator_t *navigator,
                                  const lv_point_t *point) {
    lv_area_t area;
    lv_obj_get_coords(navigator->root, &area);
    const int32_t width = lv_area_get_width(&area);
    const int32_t height = lv_area_get_height(&area);
    const int32_t radius = LV_MIN(width, height) / 2;
    const int32_t cx = area.x1 + width / 2;
    const int32_t cy = area.y1 + height / 2;
    const int64_t dx = (int64_t)point->x - cx;
    const int64_t dy = (int64_t)point->y - cy;
    return dx * dx + dy * dy <= (int64_t)radius * radius;
}

static void navigation_event_cb(lv_event_t *event) {
    dashboard_navigator_t *navigator = lv_event_get_user_data(event);
    if (navigator == NULL) return;

    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        navigator->press_started_ms = lv_tick_get();
        navigator->press_is_gesture = false;
        navigator->press_held = false;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        if (navigator->hold == NULL || navigator->press_held ||
            navigator->press_is_gesture ||
            !dashboard_period_elapsed(lv_tick_get(),
                                      navigator->press_started_ms,
                                      HOLD_ACTION_MS)) {
            return;
        }
        lv_point_t point;
        lv_indev_t *indev = lv_event_get_indev(event);
        if (indev == NULL) return;
        lv_indev_get_point(indev, &point);
        if (!point_in_visible_disc(navigator, &point)) return;
        navigator->press_held = true;
        navigator->hold(navigator->hold_context);
        return;
    }

    if (code == LV_EVENT_GESTURE) {
        lv_indev_t *indev = lv_event_get_indev(event);
        lv_point_t point;
        if (indev == NULL) return;
        lv_indev_get_point(indev, &point);
        if (!point_in_visible_disc(navigator, &point)) return;
        const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
        const uint32_t now = lv_tick_get();
        navigator->press_is_gesture = true;
        if (direction == LV_DIR_LEFT) {
            dashboard_navigator_next(navigator);
            navigator->last_gesture_ms = now;
        } else if (direction == LV_DIR_RIGHT) {
            dashboard_navigator_previous(navigator);
            navigator->last_gesture_ms = now;
        }
        return;
    }

    if (code == LV_EVENT_CLICKED) {
        // Les contrôles enfants, notamment les boutons musique, gardent leur
        // événement et leur bubbling de geste sans devenir une navigation.
        if (lv_event_get_target(event) != navigator->root) return;
        // LVGL émet CLICKED au relâcher même après un maintien : ce n'est
        // pas un tap de navigation.
        if (navigator->press_held) return;

        // LVGL peut ne pas émettre CLICKED après un geste. Le délai borné
        // expire tout de même, et la soustraction reste sûre au wrap du tick.
        const uint32_t now = lv_tick_get();
        if (!dashboard_period_elapsed(now, navigator->last_gesture_ms,
                                       GESTURE_CLICK_SUPPRESSION_MS)) {
            navigator->last_gesture_ms = now - GESTURE_CLICK_SUPPRESSION_MS;
            return;
        }

        lv_point_t point;
        lv_indev_t *indev = lv_event_get_indev(event);
        if (indev == NULL) return;
        lv_indev_get_point(indev, &point);
        if (!point_in_visible_disc(navigator, &point)) return;

        const int32_t width = lv_obj_get_width(navigator->root);
        if (point.x < lv_obj_get_x(navigator->root) + width / 3) {
            dashboard_navigator_previous(navigator);
        } else if (point.x >= lv_obj_get_x(navigator->root) +
                   (width * 2) / 3) {
            dashboard_navigator_next(navigator);
        }
    }
}

dashboard_navigator_t *dashboard_navigator_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    dashboard_navigator_t *navigator = lv_malloc(sizeof(*navigator));
    if (navigator == NULL) return NULL;
    lv_memzero(navigator, sizeof(*navigator));

    navigator->root = lv_obj_create(parent);
    if (navigator->root == NULL) {
        lv_free(navigator);
        return NULL;
    }
    lv_obj_remove_style_all(navigator->root);
    lv_obj_set_size(navigator->root, LV_PCT(100), LV_PCT(100));
    lv_obj_center(navigator->root);
    lv_obj_clear_flag(navigator->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(navigator->root, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);

    // Superposition transparente et non cliquable : elle n'intercepte aucun
    // appui (les gestes continuent d'atteindre la racine).
    navigator->indicator = lv_obj_create(navigator->root);
    if (navigator->indicator == NULL) {
        lv_obj_delete(navigator->root);
        lv_free(navigator);
        return NULL;
    }
    lv_obj_remove_style_all(navigator->indicator);
    lv_obj_set_size(navigator->indicator, LV_PCT(100), LV_PCT(100));
    lv_obj_center(navigator->indicator);
    lv_obj_clear_flag(navigator->indicator,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(navigator->indicator, navigator);
    lv_obj_add_event_cb(navigator->indicator, indicator_draw_cb,
                        LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_flag(navigator->indicator, LV_OBJ_FLAG_HIDDEN);
    navigator->last_gesture_ms = lv_tick_get() - GESTURE_CLICK_SUPPRESSION_MS;
    lv_obj_add_event_cb(navigator->root, navigation_event_cb, LV_EVENT_GESTURE, navigator);
    lv_obj_add_event_cb(navigator->root, navigation_event_cb, LV_EVENT_CLICKED, navigator);
    lv_obj_add_event_cb(navigator->root, navigation_event_cb, LV_EVENT_PRESSED, navigator);
    lv_obj_add_event_cb(navigator->root, navigation_event_cb, LV_EVENT_PRESSING, navigator);
    return navigator;
}

lv_obj_t *dashboard_navigator_create_page(dashboard_navigator_t *navigator) {
    if (navigator == NULL || navigator->count >= DASHBOARD_MAX_PAGES) return NULL;
    lv_obj_t *page = lv_obj_create(navigator->root);
    if (page == NULL) return NULL;
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
    lv_obj_center(page);
    // Conteneur transparent aux appuis : seuls les contrôles explicites des
    // pages (boutons musique) interceptent le toucher.
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    return page;
}

bool dashboard_navigator_register_page(dashboard_navigator_t *navigator,
                                       lv_obj_t *page,
                                       const dashboard_page_t *descriptor) {
    if (navigator == NULL || page == NULL || descriptor == NULL ||
        descriptor->name == NULL || navigator->count >= DASHBOARD_MAX_PAGES ||
        lv_obj_get_parent(page) != navigator->root) return false;

    navigator->pages[navigator->count] = (page_entry_t){
        .object = page,
        .descriptor = *descriptor,
    };
    if (navigator->count != 0) lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    navigator->count++;
    if (navigator->indicator != NULL) {
        lv_obj_move_foreground(navigator->indicator);
        if (navigator->count >= 2)
            lv_obj_remove_flag(navigator->indicator, LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

void dashboard_navigator_set_page_changed_callback(
    dashboard_navigator_t *navigator, dashboard_page_changed_cb_t callback,
    void *context) {
    if (navigator == NULL) return;
    navigator->page_changed = callback;
    navigator->page_changed_context = context;
}

void dashboard_navigator_set_hold_callback(dashboard_navigator_t *navigator,
                                            dashboard_hold_cb_t callback,
                                            void *context) {
    if (navigator == NULL) return;
    navigator->hold = callback;
    navigator->hold_context = context;
}

void dashboard_navigator_set_units(dashboard_navigator_t *navigator,
                                   app_settings_units_t units) {
    if (navigator == NULL || units >= APP_SETTINGS_UNITS_COUNT) return;
    for (size_t i = 0; i < navigator->count; i++) {
        if (navigator->pages[i].descriptor.settings_changed != NULL) {
            navigator->pages[i].descriptor.settings_changed(
                navigator->pages[i].descriptor.context, units);
        }
    }
}

size_t dashboard_navigator_current(const dashboard_navigator_t *navigator) {
    return navigator == NULL ? 0 : navigator->current;
}

size_t dashboard_navigator_count(const dashboard_navigator_t *navigator) {
    return navigator == NULL ? 0 : navigator->count;
}

const char *dashboard_navigator_current_name(const dashboard_navigator_t *navigator) {
    if (navigator == NULL || navigator->count == 0) return NULL;
    return navigator->pages[navigator->current].descriptor.name;
}

void dashboard_navigator_update(dashboard_navigator_t *navigator,
                                const ecu_data_t *data) {
    if (navigator == NULL || data == NULL) return;

    navigator->latest_data = *data;
    navigator->has_latest_data = true;
    update_current(navigator, false);
}

void dashboard_navigator_destroy(dashboard_navigator_t *navigator) {
    if (navigator == NULL) return;
    for (size_t i = 0; i < navigator->count; i++) {
        dashboard_page_destroy_cb_t destroy = navigator->pages[i].descriptor.destroy;
        if (destroy != NULL) destroy(navigator->pages[i].descriptor.context);
    }
    if (navigator->root != NULL) lv_obj_delete(navigator->root);
    lv_free(navigator);
}
