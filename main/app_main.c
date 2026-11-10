#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <stdbool.h>
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
#include "ui/screens/settings_screen.h"
#include "ui/navigation/dashboard_navigator.h"
#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "infrastructure/fake_ecu.h"
#include "infrastructure/kline_board_config.h"
#include "infrastructure/settings_store.h"
#include "infrastructure/settings_persistence.h"
#include "infrastructure/rtc_ds3231.h"
#include "infrastructure/rtc_worker.h"
#include "domain/app_settings.h"
#include "domain/display_brightness.h"
#include "app/dashboard_controller.h"
#include "app/settings_runtime.h"
#include "app/settings_bridge.h"
#include "app/companion_bridge.h"
#include "infrastructure/runtime_diagnostics.h"

#ifndef MGF_ENABLE_BLE_CONFIG
#define MGF_ENABLE_BLE_CONFIG 0
#endif
#ifndef MGF_BLE_CONFIG_OPEN_ON_BOOT
#define MGF_BLE_CONFIG_OPEN_ON_BOOT 0
#endif

// Un échec de démarrage ne doit pas laisser un tableau de bord éteint jusqu'à
// la prochaine coupure du contact : on libère proprement puis on redémarre.
#ifndef MGF_BOOT_FAILURE_RESTART_MS
#define MGF_BOOT_FAILURE_RESTART_MS 5000
#endif

#if MGF_USE_MEMS_KLINE
#include "infrastructure/mems_ecu.h"
// Variante ECU : MEMS_VARIANT_1_9 (MGF 1995-1999, réveil 5 bauds) par défaut,
// ou MEMS_VARIANT_1_6 (handshake direct).
#ifndef MGF_MEMS_VARIANT
#define MGF_MEMS_VARIANT MEMS_VARIANT_1_9
#endif
#endif

static const char *TAG = "mgf_gauge";

static bool disconnected_ecu_read(void *context, ecu_data_t *out) {
    (void)context;
    if (out == NULL) return false;
    *out = ecu_data_unavailable();
    return true;
}

static ecu_source_t disconnected_ecu_source(void) {
    return (ecu_source_t){
        .read = disconnected_ecu_read,
        .context = NULL,
    };
}

static rtc_t *optional_rtc_start(void) {
    shared_i2c_bus_t bus;
    if (!board_display_i2c_bus(&bus)) {
        ESP_LOGW(TAG, "RTC DS3231 désactivée: bus I2C écran indisponible");
        return NULL;
    }
    if (bus.sda_gpio != DS3231_I2C_SDA_GPIO ||
        bus.scl_gpio != DS3231_I2C_SCL_GPIO) {
        ESP_LOGW(
            TAG,
            "RTC DS3231 disabled: bus GPIO%d/GPIO%d, expected GPIO%d/GPIO%d",
            bus.sda_gpio, bus.scl_gpio, DS3231_I2C_SDA_GPIO,
            DS3231_I2C_SCL_GPIO);
        return NULL;
    }

    const ds3231_config_t config = {
        .bus = bus,
        .address = DS3231_I2C_ADDRESS,
        .timeout_ms = 100,
    };
    rtc_t *rtc = ds3231_create(&config);
    if (rtc == NULL) {
        ESP_LOGW(TAG, "RTC DS3231 non créée (configuration invalide)");
        return NULL;
    }

    const rtc_result_t result = rtc_probe(rtc);
    if (result == RTC_ERR_IO || result == RTC_ERR_INVALID_ARGUMENT) {
        ESP_LOGW(TAG, "RTC DS3231 absente ou inaccessible: %s",
                 rtc_result_name(result));
        rtc_destroy(rtc);
        return NULL;
    }
    if (result != RTC_OK) {
        ESP_LOGW(TAG, "RTC DS3231 présente mais heure à valider: %s",
                 rtc_result_name(result));
    } else {
        ESP_LOGI(TAG, "RTC DS3231 détectée sur 0x%02x", DS3231_I2C_ADDRESS);
    }
    return rtc;
}

// TTF Michroma embarqué (cf. EMBED_FILES dans main/CMakeLists.txt).
extern const uint8_t michroma_start[] asm("_binary_Michroma_Regular_ttf_start");
extern const uint8_t michroma_end[] asm("_binary_Michroma_Regular_ttf_end");

#define DEFINE_PAGE_ADAPTER(prefix)                                            \
    static void *prefix##_page_create(lv_obj_t *parent) {                      \
        return prefix##_screen_create(parent);                                 \
    }                                                                          \
    static void prefix##_page_update(void *context, const ecu_data_t *data) {  \
        prefix##_screen_update(context, data);                                 \
    }                                                                          \
    static void prefix##_page_destroy(void *context) {                         \
        prefix##_screen_destroy(context);                                      \
    }

DEFINE_PAGE_ADAPTER(clock)

// Pages alimentées par l'application compagnon (timer dédié), pas par l'ECU.
static void music_page_destroy(void *context) {
    music_screen_destroy(context);
}

static void navigation_page_destroy(void *context) {
    navigation_screen_destroy(context);
}
DEFINE_PAGE_ADAPTER(amber)
DEFINE_PAGE_ADAPTER(faults)
DEFINE_PAGE_ADAPTER(temps)
DEFINE_PAGE_ADAPTER(injection)
DEFINE_PAGE_ADAPTER(lambda)
DEFINE_PAGE_ADAPTER(ignition)
DEFINE_PAGE_ADAPTER(idle)
DEFINE_PAGE_ADAPTER(admission)

// Pages de télémétrie décrites par une table (ordre du swipe) : chaque ligne
// porte les adaptateurs générés ci-dessus, une seule boucle enregistre le tout.
typedef struct {
    const char *label;
    void *(*create)(lv_obj_t *parent);
    dashboard_page_update_cb_t update;
    dashboard_page_destroy_cb_t destroy;
    dashboard_page_settings_cb_t settings_changed;
    uint32_t update_period_ms;
} page_row_t;

#define PAGE_ROW(prefix, label, period)                                        \
    {label,                                                                    \
     prefix##_page_create,                                                     \
     prefix##_page_update,                                                     \
     prefix##_page_destroy,                                                    \
     NULL,                                                                     \
     period}
#define PAGE_ROW_UNITS(prefix, label, period, units_cb)                        \
    {label,                                                                    \
     prefix##_page_create,                                                     \
     prefix##_page_update,                                                     \
     prefix##_page_destroy,                                                    \
     units_cb,                                                                 \
     period}

static void amber_page_settings(void *context, app_settings_units_t units) {
    amber_screen_set_units(context, units);
}

static void temps_page_settings(void *context, app_settings_units_t units) {
    temps_screen_set_units(context, units);
}

static void admission_page_settings(void *context, app_settings_units_t units) {
    admission_screen_set_units(context, units);
}

void app_main(void) {
    ESP_LOGI(TAG, "MGF Gauge LVGL — ecran RPM ambre (LILYGO T-RGB H597)");

    app_settings_t settings;
    app_settings_defaults(&settings);
    esp_err_t settings_err = settings_store_init();
    if (settings_err == ESP_OK) settings_err = settings_store_load(&settings);
    if (settings_err != ESP_OK) {
        ESP_LOGW(TAG, "preferences indisponibles; defaults retained (%s)",
                 esp_err_to_name(settings_err));
    } else {
        ESP_LOGI(TAG, "application preferences loaded");
    }
    // Palette choisie avant toute création d'objet LVGL (écran de démarrage
    // compris) : aucune bascule visible au boot.
    ui_theme_set_inverted(settings.color_mode == APP_SETTINGS_COLORS_INVERTED);

    lv_display_t *display = NULL;
    bool lvgl_locked = false;
    rtc_t *rtc = NULL;
    rtc_worker_t *rtc_worker = NULL;
    settings_persistence_t *settings_persistence = NULL;
    boot_screen_t *boot = NULL;
    dashboard_navigator_t *navigator = NULL;
    dashboard_controller_t *controller = NULL;
    settings_runtime_t *settings_runtime = NULL;
    settings_bridge_t *settings_bridge = NULL;
    runtime_diagnostics_t *diagnostics = NULL;
    clock_screen_t *clock_view = NULL;
    music_screen_t *music_view = NULL;
    navigation_screen_t *navigation_view = NULL;
    bool ecu_ready = false;
#if MGF_USE_MEMS_KLINE
    mems_ecu_t *mems_ecu = NULL;
#else
    fake_ecu_t *fake_ecu = NULL;
#endif

    display = board_display_start();
    if (display == NULL) {
        ESP_LOGE(TAG,
                 "display startup failed; no optional device was required");
        goto cleanup;
    }

    board_display_backlight_set_brightness(
        display_brightness_level(settings.brightness_percent));

    // The display owns the I2C master. The DS3231 only borrows its validated
    // bus view and must be destroyed before board_display_stop().
    rtc = optional_rtc_start();

    // Hors boucle LVGL : le worker lit la RTC au démarrage puis la relit
    // périodiquement, et applique les commandes d'heure du téléphone. NULL si
    // la RTC est absente : l'horloge système reste la seule source.
    rtc_worker = rtc_worker_start(rtc);

    if (!board_display_lock(0)) {
        ESP_LOGE(TAG, "could not acquire LVGL lock");
        goto cleanup;
    }
    lvgl_locked = true;

    ui_fonts_init(michroma_start, (size_t)(michroma_end - michroma_start));
    lv_obj_t *screen = lv_screen_active();
    if (screen == NULL) {
        ESP_LOGE(TAG, "active LVGL screen is unavailable");
        goto cleanup;
    }
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    boot = boot_screen_create(screen);
    if (boot == NULL) {
        ESP_LOGE(TAG, "could not create boot screen");
        goto cleanup;
    }
    lv_refr_now(NULL);

    navigator = dashboard_navigator_create(screen);
    if (navigator == NULL) {
        ESP_LOGE(TAG, "could not create dashboard navigator");
        goto cleanup;
    }

    // Same cyclic order as the Flutter reference application.
    {
        lv_obj_t *page = dashboard_navigator_create_page(navigator);
        clock_screen_t *view = clock_screen_create(page);
        // L'écran lit uniquement l'horloge système (UTC + décalage), jamais la
        // RTC depuis l'UI : le worker maintient le système à l'heure.
        clock_screen_set_rtc(view, NULL);
        clock_view = view;
        const dashboard_page_t descriptor = {
            .name = "HEURE",
            .context = view,
            .update = clock_page_update,
            .destroy = clock_page_destroy,
            .update_period_ms = 1000,
        };
        if (page == NULL || view == NULL ||
            !dashboard_navigator_register_page(navigator, page, &descriptor)) {
            ESP_LOGE(TAG, "could not create page HEURE");
            if (view != NULL) clock_screen_destroy(view);
            goto cleanup;
        }
    }
    {
        lv_obj_t *page = dashboard_navigator_create_page(navigator);
        music_view = music_screen_create(page);
        const dashboard_page_t descriptor = {
            .name = "MUSIQUE",
            .context = music_view,
            .destroy = music_page_destroy,
        };
        if (page == NULL || music_view == NULL ||
            !dashboard_navigator_register_page(navigator, page, &descriptor)) {
            ESP_LOGE(TAG, "could not create page MUSIQUE");
            if (music_view != NULL) music_screen_destroy(music_view);
            goto cleanup;
        }
    }
    {
        lv_obj_t *page = dashboard_navigator_create_page(navigator);
        navigation_view = navigation_screen_create(page);
        const dashboard_page_t descriptor = {
            .name = "NAVIGATION",
            .context = navigation_view,
            .destroy = navigation_page_destroy,
        };
        if (page == NULL || navigation_view == NULL ||
            !dashboard_navigator_register_page(navigator, page, &descriptor)) {
            ESP_LOGE(TAG, "could not create page NAVIGATION");
            if (navigation_view != NULL)
                navigation_screen_destroy(navigation_view);
            goto cleanup;
        }
    }
    const page_row_t page_rows[] = {
        PAGE_ROW_UNITS(amber, "RPM", 40, amber_page_settings),
        PAGE_ROW(faults, "DEFAUTS", 1000),
        PAGE_ROW_UNITS(temps, "TEMPERATURES", 150, temps_page_settings),
        PAGE_ROW(injection, "INJECTION", 80),
        PAGE_ROW(lambda, "LAMBDA", 80),
        PAGE_ROW(ignition, "ALLUMAGE", 80),
        PAGE_ROW(idle, "RALENTI", 150),
        PAGE_ROW_UNITS(admission, "ADMISSION", 150, admission_page_settings),
    };
    for (size_t i = 0; i < sizeof(page_rows) / sizeof(page_rows[0]); i++) {
        const page_row_t *row = &page_rows[i];
        lv_obj_t *page = dashboard_navigator_create_page(navigator);
        void *view = page != NULL ? row->create(page) : NULL;
        const dashboard_page_t descriptor = {
            .name = row->label,
            .context = view,
            .update = row->update,
            .destroy = row->destroy,
            .settings_changed = row->settings_changed,
            .update_period_ms = row->update_period_ms,
        };
        if (page == NULL || view == NULL ||
            !dashboard_navigator_register_page(navigator, page, &descriptor)) {
            ESP_LOGE(TAG, "impossible de créer la page %s", row->label);
            if (view != NULL) row->destroy(view);
            goto cleanup;
        }
    }

    // Bilan unique après création des 11 pages (toutes gardées en mémoire) :
    // permet de suivre leur coût sans activer les diagnostics périodiques.
    ESP_LOGI(TAG, "heap after pages: internal free/min=%u/%u, psram free=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    const settings_bridge_config_t settings_bridge_config = {
        .screen = screen,
        .navigator = navigator,
        .clock = clock_view,
        .music = music_view,
    };
    settings_bridge = settings_bridge_create(&settings_bridge_config);
    // Page fixe choisie dans les réglages, sinon la dernière page vue.
    settings.selected_page = app_settings_boot_page(&settings);
    // Écritures NVS hors tâche LVGL : le coordinateur n'est acquitté qu'après
    // une sauvegarde réellement terminée.
    settings_persistence = settings_persistence_create();
    if (settings_persistence == NULL) {
        ESP_LOGE(TAG, "could not start settings persistence");
        goto cleanup;
    }
    settings_runtime = settings_runtime_create(
        &settings, settings_persistence_save, settings_persistence,
        settings_bridge_apply, settings_bridge);
    if (settings_runtime == NULL ||
        !settings_runtime_apply_current(settings_runtime)) {
        ESP_LOGE(TAG, "could not create/apply settings runtime");
        goto cleanup;
    }
    dashboard_navigator_set_page_changed_callback(
        navigator, settings_runtime_page_changed, settings_runtime);

    // Surimpression de réglages (glissement haut / maintien) + timer.
    if (!settings_bridge_start(settings_bridge, screen, settings_runtime)) {
        goto cleanup;
    }

    // Musique, navigation, heure et télécommande pilotées par le téléphone.
    const companion_bridge_config_t companion_config = {
        .navigator = navigator,
        .music = music_view,
        .navigation = navigation_view,
        .settings_runtime = settings_runtime,
        .settings_screen = settings_bridge_screen(settings_bridge),
        .rtc_worker = rtc_worker,
    };
    if (!companion_bridge_start(&companion_config)) goto cleanup;

    // A missing K-line is a disconnected ECU, not a display startup failure.
    ecu_source_t ecu_source = disconnected_ecu_source();
#if MGF_USE_MEMS_KLINE
    const mems_ecu_config_t mems_cfg = {
        .kline =
            {
                .uart_num = MGF_KLINE_UART_NUM,
                .tx_gpio = MGF_KLINE_TX_GPIO,
                .rx_gpio = MGF_KLINE_RX_GPIO,
                .baud_rate = 9600,
                .local_echo = MGF_KLINE_LOCAL_ECHO != 0,
            },
        .variant = MGF_MEMS_VARIANT,
        .poll_period_ms = 100,
    };
    mems_ecu = mems_ecu_create(&mems_cfg);
    if (mems_ecu == NULL || !mems_ecu_start(mems_ecu)) {
        ESP_LOGW(TAG, "K-line unavailable; booting with ECU disconnected");
        mems_ecu_destroy(mems_ecu);
        mems_ecu = NULL;
    } else {
        ecu_source = mems_ecu_source(mems_ecu);
        ecu_ready = true;
    }
#else
    fake_ecu = fake_ecu_create();
    if (fake_ecu == NULL || !fake_ecu_start(fake_ecu)) {
        ESP_LOGW(TAG, "simulated ECU unavailable; booting disconnected");
        fake_ecu_destroy(fake_ecu);
        fake_ecu = NULL;
    } else {
        ecu_source = fake_ecu_source(fake_ecu);
        ecu_ready = true;
    }
#endif

    const dashboard_controller_config_t controller_config = {
        .navigator = navigator,
        .ecu_source = ecu_source,
        .period_ms = 40,
    };
    controller = dashboard_controller_create(&controller_config);
    if (controller == NULL || !dashboard_controller_start(controller)) {
        ESP_LOGE(TAG, "could not start dashboard controller");
        goto cleanup;
    }

    const runtime_diagnostics_config_t diagnostics_config = {
        .period_ms = 0,
        .display_state = RUNTIME_DIAGNOSTICS_STATE_READY,
        .lvgl_state = RUNTIME_DIAGNOSTICS_STATE_READY,
        .ecu_state = ecu_ready ? RUNTIME_DIAGNOSTICS_STATE_READY
                               : RUNTIME_DIAGNOSTICS_STATE_DEGRADED,
#if MGF_ENABLE_BLE_CONFIG
        .ble_state = RUNTIME_DIAGNOSTICS_STATE_STARTING,
#else
        .ble_state = RUNTIME_DIAGNOSTICS_STATE_DISABLED,
#endif
    };
    diagnostics = runtime_diagnostics_start(&diagnostics_config);
    if (diagnostics == NULL) {
        ESP_LOGW(TAG, "runtime diagnostics disabled or unavailable");
    }

    boot_screen_destroy(boot);
    boot = NULL;
    board_display_unlock();
    lvgl_locked = false;

#if MGF_ENABLE_BLE_CONFIG
    if (settings_err == ESP_OK) {
        const esp_err_t ble_error = settings_bridge_start_ble(
            settings_runtime, rtc_worker, MGF_BLE_CONFIG_OPEN_ON_BOOT != 0);
        if (ble_error != ESP_OK) {
            ESP_LOGW(TAG, "BLE configuration unavailable: %s",
                     esp_err_to_name(ble_error));
            runtime_diagnostics_set_service_state(
                diagnostics, RUNTIME_DIAGNOSTICS_SERVICE_BLE,
                RUNTIME_DIAGNOSTICS_STATE_ERROR);
        } else {
            runtime_diagnostics_set_service_state(
                diagnostics, RUNTIME_DIAGNOSTICS_SERVICE_BLE,
                RUNTIME_DIAGNOSTICS_STATE_READY);
        }
    } else {
        ESP_LOGW(TAG, "BLE configuration disabled: NVS unavailable");
        runtime_diagnostics_set_service_state(
            diagnostics, RUNTIME_DIAGNOSTICS_SERVICE_BLE,
            RUNTIME_DIAGNOSTICS_STATE_DEGRADED);
    }
#endif
    return;

cleanup:
    // All LVGL objects and LVGL timers are stopped before the port is torn down.
    if (diagnostics != NULL) runtime_diagnostics_stop(diagnostics);
    if (controller != NULL) dashboard_controller_destroy(controller);
    // Timer compagnon puis worker de pochettes, puis timer de réglages.
    companion_bridge_destroy();
    settings_bridge_stop(settings_bridge);
    if (settings_runtime != NULL) {
        settings_runtime_destroy(settings_runtime);
    }
    settings_persistence_destroy(settings_persistence);
#if MGF_USE_MEMS_KLINE
    if (mems_ecu != NULL) mems_ecu_destroy(mems_ecu);
#else
    if (fake_ecu != NULL) fake_ecu_destroy(fake_ecu);
#endif
    settings_bridge_destroy(settings_bridge);
    if (navigator != NULL) dashboard_navigator_destroy(navigator);
    if (boot != NULL) boot_screen_destroy(boot);
    // Arrêter le worker avant de détruire la RTC et le bus I2C de l'écran.
    rtc_worker_stop(rtc_worker);
    rtc_destroy(rtc);
    if (lvgl_locked) board_display_unlock();
    if (display != NULL) {
        board_display_backlight_off();
        board_display_stop();
    }
    ESP_LOGE(TAG, "startup failed; restarting in %d ms",
             MGF_BOOT_FAILURE_RESTART_MS);
    vTaskDelay(pdMS_TO_TICKS(MGF_BOOT_FAILURE_RESTART_MS));
    esp_restart();
}
