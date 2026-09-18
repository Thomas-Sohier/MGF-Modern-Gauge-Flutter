#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

#include "gauge_theme.h"
#include "rpm_screen.h"
#include "ui_fonts.h"
#include "ecu_data.h"

static const char *TAG = "mgf_gauge";

// TTF JetBrains Mono embarqué (cf. EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t jbmono_start[] asm("_binary_JetBrainsMono_Bold_ttf_start");
extern const uint8_t jbmono_end[]   asm("_binary_JetBrainsMono_Bold_ttf_end");

// Rafraîchit l'UI depuis l'instantané ECU factice (~25 Hz).
static void ui_tick_cb(lv_timer_t *t) {
    rpm_screen_t *ui = lv_timer_get_user_data(t);
    rpm_screen_update(ui, fake_ecu_current());
}

void app_main(void) {
    ESP_LOGI(TAG, "MGF Gauge LVGL — ecran RPM (test de faisabilite ESP32-P4)");

    // Démarre l'écran MIPI-DSI + le port LVGL (thread LVGL dédié) via le BSP.
    bsp_display_start();
    bsp_display_backlight_on();

    // Toute manipulation d'objets LVGL doit se faire sous verrou (thread LVGL).
    bsp_display_lock(0);

    // Polices monospace JetBrains Mono (utilisées par le style ambre).
    ui_fonts_init(jbmono_start, (size_t)(jbmono_end - jbmono_start));

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, MGF_COL_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    rpm_screen_t *ui = rpm_screen_create(screen);

    // Modèle de données + timers de rafraîchissement (exécutés dans le thread LVGL).
    fake_ecu_start();
    lv_timer_create(ui_tick_cb, 40, ui);

    bsp_display_unlock();
}
