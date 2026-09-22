#include "esp_log.h"
#include "esp_err.h"
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
#include "ui/navigation/dashboard_navigator.h"
#include "ui/fonts/ui_fonts.h"
#include "infrastructure/fake_ecu.h"
#include "infrastructure/kline_board_config.h"
#include "infrastructure/settings_store.h"
#include "infrastructure/rtc_ds3231.h"
#include "domain/app_settings.h"
#include "domain/display_brightness.h"
#include "app/dashboard_controller.h"
#include "app/settings_runtime.h"
#include "infrastructure/runtime_diagnostics.h"

#ifndef MGF_ENABLE_BLE_CONFIG
#define MGF_ENABLE_BLE_CONFIG 0
#endif
#ifndef MGF_BLE_CONFIG_OPEN_ON_BOOT
#define MGF_BLE_CONFIG_OPEN_ON_BOOT 0
#endif

#if MGF_ENABLE_BLE_CONFIG
#include "infrastructure/ble_config_service.h"
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

#if MGF_ENABLE_BLE_CONFIG
static bool ble_settings_accept(void *context,
                                const app_settings_t *settings) {
    (void)context;
    return app_settings_is_valid(settings);
}

static rtc_result_t ble_datetime_set(void *context,
                                     const rtc_datetime_t *date_time,
                                     ble_config_time_basis_t basis) {
    // The DS3231 stores UTC only. Accepting a local civil time here without a
    // configured offset/DST policy would silently shift the clock, so keep the
    // wire-level LOCAL value parseable but reject it at the hardware boundary.
    if (context == NULL || date_time == NULL) return RTC_ERR_INVALID_ARGUMENT;
    if (basis != BLE_CONFIG_TIME_UTC) return RTC_ERR_INVALID_ARGUMENT;
    return rtc_set(context, date_time);
}
#endif

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

static bool save_settings(void *context, const app_settings_t *settings) {
    (void)context;
    const esp_err_t error = settings_store_save(settings);
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "preferences not saved: %s", esp_err_to_name(error));
        return false;
    }
    return true;
}

typedef struct {
    dashboard_navigator_t *navigator;
} settings_apply_context_t;

static settings_apply_context_t s_settings_apply_context;

static bool apply_settings(void *context, const app_settings_t *settings) {
    settings_apply_context_t *apply_context = context;
    if (apply_context == NULL || apply_context->navigator == NULL ||
        settings == NULL || !app_settings_is_valid(settings)) {
        return false;
    }
    if (!dashboard_navigator_select_page(
            apply_context->navigator, (size_t)settings->selected_page)) {
        return false;
    }
    dashboard_navigator_set_units(apply_context->navigator, settings->units);
    board_display_backlight_set_brightness(
        display_brightness_level(settings->brightness_percent));
    return true;
}

static void settings_timer_tick(lv_timer_t *timer) {
    settings_runtime_t *runtime = lv_timer_get_user_data(timer);
#if MGF_ENABLE_BLE_CONFIG
    app_settings_t pending;
    if (ble_config_service_take_settings_update(&pending)) {
        settings_runtime_update(runtime, &pending);
    }
#endif
    settings_runtime_process(runtime, lv_tick_get());
#if MGF_ENABLE_BLE_CONFIG
    if (ble_config_service_is_started()) {
        app_settings_t current;
        if (settings_runtime_read(runtime, &current)) {
            ble_config_service_sync_settings(&current);
        }
    }
#endif
}

static rtc_t *optional_rtc_start(void) {
    shared_i2c_bus_t bus;
    if (!board_display_i2c_bus(&bus)) {
        ESP_LOGW(TAG, "RTC DS3231 désactivée: bus I2C écran indisponible");
        return NULL;
    }
    if (bus.sda_gpio != DS3231_I2C_SDA_GPIO ||
        bus.scl_gpio != DS3231_I2C_SCL_GPIO) {
        ESP_LOGW(TAG, "RTC DS3231 disabled: bus GPIO%d/GPIO%d, expected GPIO%d/GPIO%d",
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
extern const uint8_t michroma_end[]   asm("_binary_Michroma_Regular_ttf_end");

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
    const dashboard_page_t descriptor = {                                     \
        .name = label, .context = view, .update = prefix##_page_update,       \
        .destroy = prefix##_page_destroy, .update_period_ms = period,         \
    };                                                                        \
    if (page == NULL || view == NULL ||                                       \
        !dashboard_navigator_register_page(navigator, page, &descriptor)) {    \
        ESP_LOGE(TAG, "impossible de créer la page %s", label);              \
        if (view != NULL) prefix##_screen_destroy(view);                      \
        goto cleanup;                                                          \
    }                                                                          \
} while (0)

static void amber_page_settings(void *context, app_settings_units_t units) {
    amber_screen_set_units(context, units);
}

static void temps_page_settings(void *context, app_settings_units_t units) {
    temps_screen_set_units(context, units);
}

static void admission_page_settings(void *context, app_settings_units_t units) {
    admission_screen_set_units(context, units);
}

#define REGISTER_PAGE_WITH_UNITS(navigator, prefix, label, period, units_cb)   \
    do {                                                                       \
        lv_obj_t *page = dashboard_navigator_create_page(navigator);           \
        prefix##_screen_t *view = prefix##_screen_create(page);                \
        const dashboard_page_t descriptor = {                                  \
            .name = label, .context = view, .update = prefix##_page_update,    \
            .destroy = prefix##_page_destroy, .settings_changed = units_cb,    \
            .update_period_ms = period,                                        \
        };                                                                     \
        if (page == NULL || view == NULL ||                                    \
            !dashboard_navigator_register_page(navigator, page, &descriptor)) { \
            ESP_LOGE(TAG, "impossible de créer la page %s", label);            \
            if (view != NULL) prefix##_screen_destroy(view);                   \
            goto cleanup;                                                       \
        }                                                                      \
    } while (0)

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

    lv_display_t *display = NULL;
    bool lvgl_locked = false;
    rtc_t *rtc = NULL;
    boot_screen_t *boot = NULL;
    dashboard_navigator_t *navigator = NULL;
    dashboard_controller_t *controller = NULL;
    settings_runtime_t *settings_runtime = NULL;
    runtime_diagnostics_t *diagnostics = NULL;
    lv_timer_t *settings_timer = NULL;
    bool ecu_ready = false;
#if MGF_USE_MEMS_KLINE
    mems_ecu_t *mems_ecu = NULL;
#else
    fake_ecu_t *fake_ecu = NULL;
#endif

    display = board_display_start();
    if (display == NULL) {
        ESP_LOGE(TAG, "display startup failed; no optional device was required");
        goto cleanup;
    }

    board_display_backlight_set_brightness(
        display_brightness_level(settings.brightness_percent));

    // The display owns the I2C master. The DS3231 only borrows its validated
    // bus view and must be destroyed before board_display_stop().
    rtc = optional_rtc_start();

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
        clock_screen_set_rtc(view, rtc);
        const dashboard_page_t descriptor = {
            .name = "HEURE", .context = view, .update = clock_page_update,
            .destroy = clock_page_destroy, .update_period_ms = 1000,
        };
        if (page == NULL || view == NULL ||
            !dashboard_navigator_register_page(navigator, page, &descriptor)) {
            ESP_LOGE(TAG, "could not create page HEURE");
            if (view != NULL) clock_screen_destroy(view);
            goto cleanup;
        }
    }
    REGISTER_PAGE(navigator, music, "MUSIQUE", 0);
    REGISTER_PAGE(navigator, navigation, "NAVIGATION", 0);
    REGISTER_PAGE_WITH_UNITS(navigator, amber, "RPM", 40,
                             amber_page_settings);
    REGISTER_PAGE(navigator, faults, "DEFAUTS", 1000);
    REGISTER_PAGE_WITH_UNITS(navigator, temps, "TEMPERATURES", 150,
                             temps_page_settings);
    REGISTER_PAGE(navigator, injection, "INJECTION", 80);
    REGISTER_PAGE(navigator, lambda, "LAMBDA", 80);
    REGISTER_PAGE(navigator, ignition, "ALLUMAGE", 80);
    REGISTER_PAGE(navigator, idle, "RALENTI", 150);
    REGISTER_PAGE_WITH_UNITS(navigator, admission, "ADMISSION", 150,
                             admission_page_settings);

    s_settings_apply_context = (settings_apply_context_t){
        .navigator = navigator,
    };
    settings_runtime = settings_runtime_create(
        &settings, save_settings, NULL, apply_settings,
        &s_settings_apply_context);
    if (settings_runtime == NULL ||
        !settings_runtime_apply_current(settings_runtime)) {
        ESP_LOGE(TAG, "could not create/apply settings runtime");
        goto cleanup;
    }
    dashboard_navigator_set_page_changed_callback(
        navigator, settings_runtime_page_changed, settings_runtime);
    settings_timer = lv_timer_create(
        settings_timer_tick, 250, settings_runtime);
    if (settings_timer == NULL) {
        ESP_LOGE(TAG, "could not create settings persistence timer");
        goto cleanup;
    }

    // A missing K-line is a disconnected ECU, not a display startup failure.
    ecu_source_t ecu_source = disconnected_ecu_source();
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
        const ble_config_service_config_t ble_config = {
            .device_name = "MGF Gauge",
            .settings_context = settings_runtime,
            .settings_read = settings_runtime_read,
            .settings_update = ble_settings_accept,
            .rtc_context = rtc,
            .datetime_set = ble_datetime_set,
            .open_on_start = MGF_BLE_CONFIG_OPEN_ON_BOOT != 0,
        };
        const esp_err_t ble_error = ble_config_service_start(&ble_config);
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
    if (settings_timer != NULL) lv_timer_delete(settings_timer);
    if (settings_runtime != NULL) {
        settings_runtime_destroy(settings_runtime);
    }
#if MGF_USE_MEMS_KLINE
    if (mems_ecu != NULL) mems_ecu_destroy(mems_ecu);
#else
    if (fake_ecu != NULL) fake_ecu_destroy(fake_ecu);
#endif
    if (navigator != NULL) dashboard_navigator_destroy(navigator);
    if (boot != NULL) boot_screen_destroy(boot);
    rtc_destroy(rtc);
    if (lvgl_locked) board_display_unlock();
    if (display != NULL) {
        board_display_backlight_off();
        board_display_stop();
    }
}
