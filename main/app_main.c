#include "esp_log.h"
#include "lvgl.h"

#include <stddef.h>
#include <stdint.h>

#include "infrastructure/board_display.h"
#include "ui/screens/style_amber.h"
#include "ui/screens/boot_screen.h"
#include "ui/screens/clock_screen.h"
#include "ui/screens/music_screen.h"
#include "ui/screens/navigation_screen.h"
#include "ui/screens/faults_screen.h"
#include "ui/screens/temps_screen.h"
#include "ui/screens/injection_screen.h"
#include "ui/screens/lambda_screen.h"
#include "ui/screens/ignition_screen.h"
#include "ui/screens/idle_screen.h"
#include "ui/screens/admission_screen.h"
#include "ui/navigation/dashboard_navigator.h"
#include "ui/fonts/ui_fonts.h"
#include "infrastructure/fake_ecu.h"
#include "infrastructure/kline_board_config.h"
#include "app/dashboard_controller.h"

#if MGF_USE_MEMS_KLINE
#include "infrastructure/mems_ecu.h"
// Variante ECU : MEMS_VARIANT_1_6 (direct) ou MEMS_VARIANT_1_9 (réveil 5 bauds).
#ifndef MGF_MEMS_VARIANT
#define MGF_MEMS_VARIANT MEMS_VARIANT_1_6
#endif
#endif

static const char *TAG = "mgf_gauge";

// TTF Michroma embarqué (cf. EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t michroma_start[] asm("_binary_fonts_Michroma_Regular_ttf_start");
extern const uint8_t michroma_end[]   asm("_binary_fonts_Michroma_Regular_ttf_end");

#define DEFINE_PAGE_ADAPTER(prefix)                                           \
    static void prefix##_page_update(void *context, const ecu_data_t *data) { \
        prefix##_screen_update(context, data);                                \
    }                                                                         \
    static void prefix##_page_destroy(void *context) {                        \
        prefix##_screen_destroy(context);                                     \
    }

DEFINE_PAGE_ADAPTER(clock)
DEFINE_PAGE_ADAPTER(music)
DEFINE_PAGE_ADAPTER(navigation)
DEFINE_PAGE_ADAPTER(amber)
DEFINE_PAGE_ADAPTER(faults)
DEFINE_PAGE_ADAPTER(temps)
DEFINE_PAGE_ADAPTER(injection)
DEFINE_PAGE_ADAPTER(lambda)
DEFINE_PAGE_ADAPTER(ignition)
DEFINE_PAGE_ADAPTER(idle)
DEFINE_PAGE_ADAPTER(admission)

#define REGISTER_PAGE(navigator, prefix, label, period) do {                  \
    lv_obj_t *page = dashboard_navigator_create_page(navigator);              \
    prefix##_screen_t *view = prefix##_screen_create(page);                   \
    const dashboard_page_t descriptor = {                                    \
        .name = label, .context = view, .update = prefix##_page_update,        \
        .destroy = prefix##_page_destroy, .update_period_ms = period,          \
    };                                                                         \
    if (page == NULL || view == NULL ||                                       \
        !dashboard_navigator_register_page(navigator, page, &descriptor)) {    \
        ESP_LOGE(TAG, "impossible de créer la page %s", label);              \
        dashboard_navigator_destroy(navigator);                               \
        boot_screen_destroy(boot);                                            \
        board_display_unlock();                                               \
        board_display_backlight_off();                                        \
        return;                                                               \
    }                                                                          \
} while (0)

void app_main(void) {
    ESP_LOGI(TAG, "MGF Gauge LVGL — ecran RPM ambre (LILYGO T-RGB H597)");

    // Écran ST7701S RGB 480x480 rond + tactile CST820 + port LVGL (thread dédié).
    if (board_display_start() == NULL) {
        ESP_LOGE(TAG, "impossible d'initialiser l'écran");
        board_display_backlight_off();
        return;
    }
    board_display_backlight_on();

    // Toute manipulation d'objets LVGL doit se faire sous verrou (thread LVGL).
    if (!board_display_lock(0)) {
        ESP_LOGE(TAG, "impossible de prendre le verrou LVGL");
        board_display_backlight_off();
        return;
    }

    // Police Michroma (carrée, esprit Microgramma ; utilisée par le style ambre).
    ui_fonts_init(michroma_start, (size_t)(michroma_end - michroma_start));

    lv_obj_t *screen = lv_screen_active();
    if (screen == NULL) {
        ESP_LOGE(TAG, "écran LVGL actif introuvable");
        board_display_unlock();
        board_display_backlight_off();
        return;
    }
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // Affichage immédiat pendant la préparation de la source et du contrôleur.
    boot_screen_t *boot = boot_screen_create(screen);
    if (boot == NULL) {
        ESP_LOGE(TAG, "impossible de créer l'écran de démarrage");
        board_display_unlock();
        board_display_backlight_off();
        return;
    }
    lv_refr_now(NULL);

    dashboard_navigator_t *navigator = dashboard_navigator_create(screen);
    if (navigator == NULL) {
        ESP_LOGE(TAG, "impossible de créer la navigation du dashboard");
        boot_screen_destroy(boot);
        board_display_unlock();
        board_display_backlight_off();
        return;
    }

    // Même ordre cyclique que l'application Flutter de référence.
    REGISTER_PAGE(navigator, clock, "HEURE", 1000);
    REGISTER_PAGE(navigator, music, "MUSIQUE", 0);
    REGISTER_PAGE(navigator, navigation, "NAVIGATION", 0);
    REGISTER_PAGE(navigator, amber, "RPM", 40);
    REGISTER_PAGE(navigator, faults, "DEFAUTS", 1000);
    REGISTER_PAGE(navigator, temps, "TEMPERATURES", 150);
    REGISTER_PAGE(navigator, injection, "INJECTION", 80);
    REGISTER_PAGE(navigator, lambda, "LAMBDA", 80);
    REGISTER_PAGE(navigator, ignition, "ALLUMAGE", 80);
    REGISTER_PAGE(navigator, idle, "RALENTI", 150);
    REGISTER_PAGE(navigator, admission, "ADMISSION", 150);

    // Composition de l'application : la source est branchée au contrôleur,
    // qui possède le snapshot et orchestre le rafraîchissement de l'écran.
    // Les deux sources exposent le même contrat `ecu_source_t` ; le contrôleur
    // ne lit qu'une copie non bloquante du dernier instantané.
    ecu_source_t ecu_source;
#if MGF_USE_MEMS_KLINE
    const mems_ecu_config_t mems_cfg = {
        .kline = {
            .uart_num = MGF_KLINE_UART_NUM,
            .tx_gpio = MGF_KLINE_TX_GPIO,
            .rx_gpio = MGF_KLINE_RX_GPIO,
            .baud_rate = 9600,
            .local_echo = MGF_KLINE_LOCAL_ECHO != 0,
        },
        .variant = MGF_MEMS_VARIANT,
        .poll_period_ms = 200,
    };
    mems_ecu_t *mems_ecu = mems_ecu_create(&mems_cfg);
    if (mems_ecu == NULL || !mems_ecu_start(mems_ecu)) {
        ESP_LOGE(TAG, "impossible de démarrer la source MEMS K-line");
        mems_ecu_destroy(mems_ecu);
        dashboard_navigator_destroy(navigator);
        boot_screen_destroy(boot);
        board_display_unlock();
        board_display_backlight_off();
        return;
    }
    ecu_source = mems_ecu_source(mems_ecu);
#else
    fake_ecu_t *fake_ecu = fake_ecu_create();
    if (fake_ecu == NULL || !fake_ecu_start(fake_ecu)) {
        ESP_LOGE(TAG, "impossible de démarrer la source ECU factice");
        fake_ecu_destroy(fake_ecu);
        dashboard_navigator_destroy(navigator);
        boot_screen_destroy(boot);
        board_display_unlock();
        board_display_backlight_off();
        return;
    }
    ecu_source = fake_ecu_source(fake_ecu);
#endif
    const dashboard_controller_config_t controller_config = {
        .navigator = navigator,
        .ecu_source = ecu_source,
        .period_ms = 40,
    };
    dashboard_controller_t *controller =
        dashboard_controller_create(&controller_config);
    if (controller == NULL || !dashboard_controller_start(controller)) {
        ESP_LOGE(TAG, "impossible de démarrer le contrôleur dashboard");
        dashboard_controller_destroy(controller);
#if MGF_USE_MEMS_KLINE
        mems_ecu_destroy(mems_ecu);
#else
        fake_ecu_destroy(fake_ecu);
#endif
        dashboard_navigator_destroy(navigator);
        boot_screen_destroy(boot);
        board_display_unlock();
        board_display_backlight_off();
        return;
    }

    boot_screen_destroy(boot);
    board_display_unlock();
}
