#include "esp_log.h"
#include "lvgl.h"

#include "board_display.h"
#include "style_amber.h"
#include "ui_fonts.h"
#include "ecu_data.h"

static const char *TAG = "mgf_gauge";

// TTF Michroma embarqué (cf. EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t michroma_start[] asm("_binary_Michroma_Regular_ttf_start");
extern const uint8_t michroma_end[]   asm("_binary_Michroma_Regular_ttf_end");

// Rafraîchit l'UI depuis l'instantané ECU factice (~25 Hz).
static void ui_tick_cb(lv_timer_t *t) {
    amber_screen_t *ui = lv_timer_get_user_data(t);
    amber_screen_update(ui, fake_ecu_current());
}

void app_main(void) {
    ESP_LOGI(TAG, "MGF Gauge LVGL — ecran RPM ambre (ESP32-S3-Touch-LCD-2.1)");

    // Écran ST7701 RGB 480x480 rond + tactile CST820 + port LVGL (thread dédié).
    board_display_start();
    board_display_backlight_on();

    // Toute manipulation d'objets LVGL doit se faire sous verrou (thread LVGL).
    board_display_lock(0);

    // Police Michroma (carrée, esprit Microgramma ; utilisée par le style ambre).
    ui_fonts_init(michroma_start, (size_t)(michroma_end - michroma_start));

    lv_obj_t *screen = lv_screen_active();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // L'écran ambre est carré/centré ; il occupe le disque 480x480.
    amber_screen_t *ui = amber_screen_create(screen);

    // Modèle de données + timer de rafraîchissement (thread LVGL).
    fake_ecu_start();
    lv_timer_create(ui_tick_cb, 40, ui);

    board_display_unlock();
}
