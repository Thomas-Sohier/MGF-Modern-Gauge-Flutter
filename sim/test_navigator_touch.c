// Test d'intégration tactile du navigateur contre le vrai LVGL du simulateur :
// un pointeur virtuel rejoue taps, maintiens et glissements, lus toutes les
// 30 ms comme sur la cible (CONFIG_LV_DEF_REFR_PERIOD). Vérifie que les gestes
// atteignent bien la racine du navigateur (horizontal = page, haut = réglages)
// sans casser le tap latéral ni le maintien.

#include "lvgl.h"
#include "ui/navigation/dashboard_hold.h"
#include "ui/navigation/dashboard_navigator.h"
#include "ui/ui_layout.h"

#include <stdio.h>
#include <stdlib.h>

#define READ_PERIOD_MS 30U
#define PAGE_COUNT     3U

#define CHECK(condition)                                                       \
    do {                                                                       \
        if (!(condition)) {                                                    \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);    \
            exit(1);                                                           \
        }                                                                      \
    } while (0)

static uint32_t s_now_ms = 1000U;
static lv_indev_data_t s_touch;
static lv_indev_t *s_indev;
static unsigned s_settings_requests;

static uint32_t tick_cb(void) {
    return s_now_ms;
}

static void flush_cb(lv_display_t *display, const lv_area_t *area,
                     uint8_t *px_map) {
    LV_UNUSED(area);
    LV_UNUSED(px_map);
    lv_display_flush_ready(display);
}

static void touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
    LV_UNUSED(indev);
    *data = s_touch;
}

static void settings_requested(void *context) {
    LV_UNUSED(context);
    s_settings_requests++;
}

// Une lecture tactile, puis avance du temps d'une période de lecture.
static void sample(int32_t x, int32_t y, bool pressed) {
    s_touch.point.x = x;
    s_touch.point.y = y;
    s_touch.state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    lv_indev_read(s_indev);
    s_now_ms += READ_PERIOD_MS;
}

// Glissement linéaire en `steps` lectures, doigt relevé à l'arrivée.
static void swipe(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                  int32_t steps) {
    for (int32_t i = 0; i <= steps; i++) {
        sample(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps, true);
    }
    sample(x1, y1, false);
    sample(x1, y1, false);
}

static void press_for(int32_t x, int32_t y, uint32_t duration_ms) {
    for (uint32_t elapsed = 0; elapsed <= duration_ms;
         elapsed += READ_PERIOD_MS) {
        sample(x, y, true);
    }
    sample(x, y, false);
    sample(x, y, false);
}

static void tap(int32_t x, int32_t y) {
    press_for(x, y, 60U);
}

// Laisse expirer la fenêtre anti-tap qui suit un geste.
static void settle(void) {
    s_now_ms += 1000U;
}

static void reset(dashboard_navigator_t *navigator) {
    CHECK(dashboard_navigator_select_page(navigator, 0));
    s_settings_requests = 0;
    settle();
}

static void test_horizontal_swipes_change_page(dashboard_navigator_t *nav) {
    reset(nav);
    swipe(380, 240, 100, 240, 8);
    CHECK(dashboard_navigator_current(nav) == 1);
    settle();
    swipe(100, 240, 380, 240, 8);
    CHECK(dashboard_navigator_current(nav) == 0);
    CHECK(s_settings_requests == 0);
}

static void test_swipe_up_opens_settings(dashboard_navigator_t *nav) {
    reset(nav);
    swipe(240, 380, 240, 100, 8);
    CHECK(s_settings_requests == 1);
    CHECK(dashboard_navigator_current(nav) == 0);

    // Relâché dans le tiers droit : ce n'est pas un tap de navigation.
    settle();
    swipe(400, 360, 400, 160, 8);
    CHECK(s_settings_requests == 2);
    CHECK(dashboard_navigator_current(nav) == 0);
}

// Glissement posé (~270 px/s) : au-dessus de la vitesse minimale LVGL.
static void test_unhurried_swipe_up_opens_settings(dashboard_navigator_t *nav) {
    reset(nav);
    swipe(240, 330, 240, 170, 20);
    CHECK(s_settings_requests == 1);
    CHECK(dashboard_navigator_current(nav) == 0);
}

static void test_swipe_down_is_ignored(dashboard_navigator_t *nav) {
    reset(nav);
    swipe(240, 100, 240, 380, 8);
    CHECK(s_settings_requests == 0);
    CHECK(dashboard_navigator_current(nav) == 0);
}

static void test_swipe_outside_disc_is_ignored(dashboard_navigator_t *nav) {
    reset(nav);
    // Coin haut gauche, hors de la dalle ronde.
    swipe(40, 120, 40, 10, 6);
    CHECK(s_settings_requests == 0);
    CHECK(dashboard_navigator_current(nav) == 0);
}

static void test_taps_still_navigate(dashboard_navigator_t *nav) {
    reset(nav);
    tap(420, 240);
    CHECK(dashboard_navigator_current(nav) == 1);
    tap(60, 240);
    CHECK(dashboard_navigator_current(nav) == 0);
    tap(240, 240);
    CHECK(dashboard_navigator_current(nav) == 0);
    CHECK(s_settings_requests == 0);
}

static void test_hold_still_opens_settings(dashboard_navigator_t *nav) {
    reset(nav);
    press_for(420, 240, DASHBOARD_HOLD_MS + 2U * READ_PERIOD_MS);
    CHECK(s_settings_requests == 1);
    // Le relâcher du maintien n'est pas un tap sur le tiers droit.
    CHECK(dashboard_navigator_current(nav) == 0);
}

int main(void) {
    lv_init();
    lv_tick_set_cb(tick_cb);

    static uint8_t buffer[UI_DISPLAY_SIZE_PX * 10 * 4];
    lv_display_t *display =
        lv_display_create(UI_DISPLAY_SIZE_PX, UI_DISPLAY_SIZE_PX);
    lv_display_set_buffers(display, buffer, NULL, sizeof(buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush_cb);

    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, touch_read);
    lv_indev_set_display(s_indev, display);
    // Lectures pilotées par le test uniquement.
    lv_timer_pause(lv_indev_get_read_timer(s_indev));

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    dashboard_navigator_t *nav = dashboard_navigator_create(screen);
    CHECK(nav != NULL);
    for (unsigned i = 0; i < PAGE_COUNT; i++) {
        lv_obj_t *page = dashboard_navigator_create_page(nav);
        const dashboard_page_t descriptor = {.name = "PAGE"};
        CHECK(dashboard_navigator_register_page(nav, page, &descriptor));
    }
    dashboard_navigator_set_settings_callback(nav, settings_requested, NULL);
    lv_obj_update_layout(screen);

    test_horizontal_swipes_change_page(nav);
    test_swipe_up_opens_settings(nav);
    test_unhurried_swipe_up_opens_settings(nav);
    test_swipe_down_is_ignored(nav);
    test_swipe_outside_disc_is_ignored(nav);
    test_taps_still_navigate(nav);
    test_hold_still_opens_settings(nav);

    dashboard_navigator_destroy(nav);
    puts("navigator touch tests: OK");
    return 0;
}
