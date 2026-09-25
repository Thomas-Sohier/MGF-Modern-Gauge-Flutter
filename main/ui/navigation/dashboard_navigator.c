#include "ui/navigation/dashboard_navigator.h"
#include "domain/temperature_status.h"
#include "ui/navigation/dashboard_hold.h"
#include "ui/navigation/dashboard_timing.h"
#include "ui/navigation/ui_instrumentation.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_kit.h"

#include <math.h>

#define DASHBOARD_MAX_PAGES          12
#define GESTURE_CLICK_SUPPRESSION_MS 300U

// Bandeau d'alerte thermique : bandeau opaque posé sur la ligne d'état basse,
// dans le repère logique 320 (largeur 192, hauteur 22, centre (160, 272)).
// Aucun padding : le texte est centré sur toute la surface.
#define THERMAL_ALERT_W      192.0f
#define THERMAL_ALERT_H      22.0f
#define THERMAL_ALERT_CX     160.0f
#define THERMAL_ALERT_CY     272.0f
#define THERMAL_ALERT_RADIUS 4.0f
#define THERMAL_ALERT_TEXT   "ALERTE THERMIQUE"

typedef struct {
    lv_obj_t *object;
    dashboard_page_t descriptor;
    uint32_t last_update_ms;
    bool has_presented_data;
} page_entry_t;

struct dashboard_navigator_s {
    lv_obj_t *root;
    lv_obj_t *thermal_alert;
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
    dashboard_hold_t press_hold;
};

static void update_current(dashboard_navigator_t *navigator, bool force) {
    if (!navigator->has_latest_data || navigator->count == 0) return;

    page_entry_t *page = &navigator->pages[navigator->current];
    const uint32_t period = page->descriptor.update_period_ms;
    if (!force &&
        (period == 0 || (page->has_presented_data &&
                         !dashboard_period_elapsed(
                             lv_tick_get(), page->last_update_ms, period)))) {
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
        if (i == navigator->current)
            lv_obj_remove_flag(navigator->pages[i].object, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(navigator->pages[i].object, LV_OBJ_FLAG_HIDDEN);
    }
    // La pile de pages recouvre le bandeau : remettre l'alerte au premier plan
    // après toute bascule, afin qu'elle reste visible sur toutes les pages
    // (musique/navigation/horloge comprises).
    if (navigator->thermal_alert != NULL) {
        lv_obj_move_foreground(navigator->thermal_alert);
    }
}

// Bandeau créé caché : seule la ligne d'état est recouverte, jamais la valeur
// héros ni la zone de geste. Non cliquable pour ne pas capter les appuis.
static lv_obj_t *thermal_alert_create(lv_obj_t *root) {
    if (root == NULL) return NULL;
    lv_obj_t *alert = lv_obj_create(root);
    if (alert == NULL) return NULL;
    lv_obj_remove_style_all(alert);
    lv_obj_clear_flag(alert, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(alert, ui_theme_amber_bright(), 0);
    lv_obj_set_style_bg_opa(alert, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(alert, 0, 0);
    lv_obj_set_style_pad_all(alert, 0, 0);

    lv_obj_update_layout(root);
    const ui_layout_t layout =
        ui_layout_fit(lv_obj_get_width(root), lv_obj_get_height(root));
    const int32_t width = (int32_t)lroundf(THERMAL_ALERT_W * layout.scale);
    const int32_t height = (int32_t)lroundf(THERMAL_ALERT_H * layout.scale);
    const int32_t cx = (int32_t)lroundf(ui_layout_x(&layout, THERMAL_ALERT_CX));
    const int32_t cy = (int32_t)lroundf(ui_layout_y(&layout, THERMAL_ALERT_CY));
    lv_obj_set_size(alert, width, height);
    lv_obj_set_pos(alert, cx - width / 2, cy - height / 2);
    lv_obj_set_style_radius(
        alert, (int32_t)lroundf(THERMAL_ALERT_RADIUS * layout.scale), 0);

    lv_obj_t *label = amber_kit_label(alert, amber_kit_font_caption(),
                                      ui_theme_amber_bg(), THERMAL_ALERT_TEXT);
    if (label == NULL) {
        lv_obj_delete(alert);
        return NULL;
    }
    lv_obj_center(label);
    lv_obj_add_flag(alert, LV_OBJ_FLAG_HIDDEN);
    return alert;
}

static void thermal_alert_apply(dashboard_navigator_t *navigator, bool hot) {
    if (navigator->thermal_alert == NULL) return;
    if (hot)
        lv_obj_remove_flag(navigator->thermal_alert, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(navigator->thermal_alert, LV_OBJ_FLAG_HIDDEN);
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
    dashboard_navigator_select_page(navigator, (navigator->current + 1) %
                                                   navigator->count);
}

void dashboard_navigator_previous(dashboard_navigator_t *navigator) {
    if (navigator == NULL || navigator->count < 2) return;
    dashboard_navigator_select_page(
        navigator,
        (navigator->current + navigator->count - 1) % navigator->count);
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
        // Mémorise le point de départ : c'est lui qui servira de repère pour
        // distinguer un maintien immobile d'un glissement lent.
        lv_indev_t *indev = lv_event_get_indev(event);
        if (indev == NULL) {
            dashboard_hold_reset(&navigator->press_hold);
            return;
        }
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        dashboard_hold_begin(&navigator->press_hold, lv_tick_get(), point.x,
                             point.y);
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        lv_indev_t *indev = lv_event_get_indev(event);
        if (indev == NULL) return;
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        const int32_t min_dimension =
            LV_MIN(lv_obj_get_width(navigator->root),
                   lv_obj_get_height(navigator->root));
        // Un glissement au-delà du seuil annule le maintien pour toute la
        // pression : revenir au point de départ ne le réarme pas.
        dashboard_hold_move_cancelled(&navigator->press_hold, point.x, point.y,
                                      min_dimension);
        if (navigator->hold == NULL ||
            !dashboard_hold_should_fire(
                &navigator->press_hold, lv_tick_get(),
                point_in_visible_disc(navigator, &point)))
            return;
        navigator->hold(navigator->hold_context);
        return;
    }

    if (code == LV_EVENT_GESTURE) {
        // Un geste LVGL annule aussi le maintien de cette pression.
        dashboard_hold_cancel(&navigator->press_hold);
        lv_indev_t *indev = lv_event_get_indev(event);
        lv_point_t point;
        if (indev == NULL) return;
        lv_indev_get_point(indev, &point);
        if (!point_in_visible_disc(navigator, &point)) return;
        const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
        const uint32_t now = lv_tick_get();
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
        // pas un tap de navigation. Un glissement lent qui a annulé le
        // maintien ne doit pas non plus déclencher de tap en dérivant.
        if (navigator->press_hold.fired || navigator->press_hold.cancelled)
            return;

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
        } else if (point.x >= lv_obj_get_x(navigator->root) + (width * 2) / 3) {
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
    lv_obj_add_flag(navigator->root,
                    LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);

    navigator->thermal_alert = thermal_alert_create(navigator->root);
    if (navigator->thermal_alert == NULL) {
        lv_obj_delete(navigator->root);
        lv_free(navigator);
        return NULL;
    }

    navigator->last_gesture_ms = lv_tick_get() - GESTURE_CLICK_SUPPRESSION_MS;
    lv_obj_add_event_cb(navigator->root, navigation_event_cb, LV_EVENT_GESTURE,
                        navigator);
    lv_obj_add_event_cb(navigator->root, navigation_event_cb, LV_EVENT_CLICKED,
                        navigator);
    lv_obj_add_event_cb(navigator->root, navigation_event_cb, LV_EVENT_PRESSED,
                        navigator);
    lv_obj_add_event_cb(navigator->root, navigation_event_cb, LV_EVENT_PRESSING,
                        navigator);
    return navigator;
}

lv_obj_t *dashboard_navigator_create_page(dashboard_navigator_t *navigator) {
    if (navigator == NULL || navigator->count >= DASHBOARD_MAX_PAGES)
        return NULL;
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
        lv_obj_get_parent(page) != navigator->root)
        return false;

    navigator->pages[navigator->count] = (page_entry_t){
        .object = page,
        .descriptor = *descriptor,
    };
    if (navigator->count != 0) lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
    navigator->count++;
    if (navigator->thermal_alert != NULL) {
        lv_obj_move_foreground(navigator->thermal_alert);
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

const char *
dashboard_navigator_current_name(const dashboard_navigator_t *navigator) {
    if (navigator == NULL || navigator->count == 0) return NULL;
    return navigator->pages[navigator->current].descriptor.name;
}

void dashboard_navigator_update(dashboard_navigator_t *navigator,
                                const ecu_data_t *data) {
    if (navigator == NULL || data == NULL) return;

    navigator->latest_data = *data;
    navigator->has_latest_data = true;
    // Statut calculé sur le seul instantané ECU, indépendamment de la page
    // visible et de son éventuel callback de mise à jour : le bandeau couvre
    // aussi les pages événementielles (musique, navigation, horloge).
    thermal_alert_apply(navigator,
                        temperature_status_of(&navigator->latest_data) ==
                            TEMPERATURE_STATUS_THERMAL_ALERT);
    update_current(navigator, false);
}

void dashboard_navigator_destroy(dashboard_navigator_t *navigator) {
    if (navigator == NULL) return;
    for (size_t i = 0; i < navigator->count; i++) {
        dashboard_page_destroy_cb_t destroy =
            navigator->pages[i].descriptor.destroy;
        if (destroy != NULL) destroy(navigator->pages[i].descriptor.context);
    }
    if (navigator->root != NULL) lv_obj_delete(navigator->root);
    lv_free(navigator);
}
