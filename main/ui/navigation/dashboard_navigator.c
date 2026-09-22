#include "ui/navigation/dashboard_navigator.h"

#define DASHBOARD_MAX_PAGES 12

typedef struct {
    lv_obj_t *object;
    dashboard_page_t descriptor;
    uint32_t last_update_ms;
    bool has_presented_data;
} page_entry_t;

struct dashboard_navigator_s {
    lv_obj_t *root;
    page_entry_t pages[DASHBOARD_MAX_PAGES];
    size_t count;
    size_t current;
    ecu_data_t latest_data;
    bool has_latest_data;
    bool ignore_click_after_gesture;
};

static void update_current(dashboard_navigator_t *navigator, bool force) {
    if (!navigator->has_latest_data || navigator->count == 0) return;

    page_entry_t *page = &navigator->pages[navigator->current];
    const uint32_t period = page->descriptor.update_period_ms;
    if (!force && (period == 0 ||
                   (page->has_presented_data &&
                    lv_tick_elaps(page->last_update_ms) < period))) {
        return;
    }

    page->last_update_ms = lv_tick_get();
    page->has_presented_data = true;
    if (page->descriptor.update != NULL) {
        page->descriptor.update(page->descriptor.context,
                                &navigator->latest_data);
    }
}

static void show_current(dashboard_navigator_t *navigator) {
    for (size_t i = 0; i < navigator->count; i++) {
        if (i == navigator->current) lv_obj_remove_flag(navigator->pages[i].object, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(navigator->pages[i].object, LV_OBJ_FLAG_HIDDEN);
    }
}

void dashboard_navigator_next(dashboard_navigator_t *navigator) {
    if (navigator == NULL || navigator->count < 2) return;
    navigator->current = (navigator->current + 1) % navigator->count;
    show_current(navigator);
    update_current(navigator, true);
}

void dashboard_navigator_previous(dashboard_navigator_t *navigator) {
    if (navigator == NULL || navigator->count < 2) return;
    navigator->current = (navigator->current + navigator->count - 1) % navigator->count;
    show_current(navigator);
    update_current(navigator, true);
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

    if (lv_event_get_code(event) == LV_EVENT_GESTURE) {
        lv_indev_t *indev = lv_event_get_indev(event);
        lv_point_t point;
        if (indev == NULL) return;
        lv_indev_get_point(indev, &point);
        if (!point_in_visible_disc(navigator, &point)) return;
        const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
        if (direction == LV_DIR_LEFT) {
            dashboard_navigator_next(navigator);
            navigator->ignore_click_after_gesture = true;
        } else if (direction == LV_DIR_RIGHT) {
            dashboard_navigator_previous(navigator);
            navigator->ignore_click_after_gesture = true;
        }
        return;
    }

    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        if (navigator->ignore_click_after_gesture) {
            navigator->ignore_click_after_gesture = false;
            return;
        }

        // Les contrôles enfants, notamment les boutons musique, gardent leur
        // événement et leur bubbling de geste sans devenir une navigation.
        if (lv_event_get_target(event) != navigator->root) return;

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
    lv_obj_add_event_cb(navigator->root, navigation_event_cb, LV_EVENT_GESTURE, navigator);
    lv_obj_add_event_cb(navigator->root, navigation_event_cb, LV_EVENT_CLICKED, navigator);
    return navigator;
}

lv_obj_t *dashboard_navigator_create_page(dashboard_navigator_t *navigator) {
    if (navigator == NULL || navigator->count >= DASHBOARD_MAX_PAGES) return NULL;
    lv_obj_t *page = lv_obj_create(navigator->root);
    if (page == NULL) return NULL;
    lv_obj_remove_style_all(page);
    lv_obj_set_size(page, LV_PCT(100), LV_PCT(100));
    lv_obj_center(page);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
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
    return true;
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
