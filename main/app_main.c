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
#include <sys/time.h>

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
#include "domain/ble_window.h"
#include "domain/companion_protocol.h"
#include "domain/rtc_time.h"
#include "domain/display_brightness.h"
#include "domain/music_cover.h"
#include "app/dashboard_controller.h"
#include "app/settings_runtime.h"
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

#if MGF_ENABLE_BLE_CONFIG
#include "infrastructure/ble_config_service.h"
#include "infrastructure/companion_art_worker.h"
#include "infrastructure/companion_gatt.h"
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
static bool ble_settings_accept(void *context, const app_settings_t *settings) {
    (void)context;
    return app_settings_is_valid(settings);
}

static rtc_result_t ble_datetime_set(void *context,
                                     const rtc_datetime_t *date_time,
                                     ble_config_time_basis_t basis) {
    // The DS3231 stores UTC only. Accepting a local civil time here without a
    // configured offset/DST policy would silently shift the clock, so keep the
    // wire-level LOCAL value parseable but reject it at the hardware boundary.
    if (date_time == NULL || basis != BLE_CONFIG_TIME_UTC ||
        !rtc_datetime_is_valid(date_time)) {
        return RTC_ERR_INVALID_ARGUMENT;
    }

    // `context` est le worker RTC (rtc_worker_t*). On publie d'abord la
    // commande : rtc_worker_submit marque phone_seen (priorité téléphone pour
    // toute la session) sous le même verrou que la mise en file. Tant que ce
    // n'est pas acquis (context NULL ou submit refusé), on ne touche PAS à
    // l'horloge système : aucune mise à l'heure orpheline, RTC_ERR_IO seul.
    if (!rtc_worker_submit(context, date_time)) {
        ESP_LOGW(TAG, "heure BLE refusée (RTC absente) : système intact");
        return RTC_ERR_IO;
    }

    // phone_seen est désormais publié : le recalage périodique RTC ne peut plus
    // écraser cette heure. Mise à l'heure système APRÈS l'acquittement en file.
    int64_t seconds = 0;
    if (rtc_datetime_to_unix(date_time, &seconds)) {
        const struct timeval now = {
            .tv_sec = (time_t)seconds,
            .tv_usec = 0,
        };
        settimeofday(&now, NULL);
    }

    // RTC_OK signifie que la commande est ACCEPTÉE et mise en file ; l'écriture
    // I2C n'est pas encore garantie.
    return RTC_OK;
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

typedef struct {
    lv_obj_t *screen; // écran actif : navigateur + surimpression réglages
    dashboard_navigator_t *navigator;
    clock_screen_t *clock;
    music_screen_t *music;
} settings_apply_context_t;

static settings_apply_context_t s_settings_apply_context;

// Palette normale/inversée appliquée instantanément, sans recréer les écrans
// (média, pochette, itinéraire et page courante sont conservés) : les styles
// locaux sont remappés rôle par rôle et les callbacks de dessin relisent la
// palette active. No-op si la palette est déjà la bonne.
static void apply_color_mode(settings_apply_context_t *context,
                             app_settings_color_mode_t mode) {
    const bool inverted = mode == APP_SETTINGS_COLORS_INVERTED;
    const bool from = ui_theme_is_inverted();
    if (!ui_theme_apply_inverted(context->screen, inverted)) return;
    // La pochette affichée appartient à l'écran musique : ses pixels sont
    // convertis sans perte (music_cover_convert_palette) par
    // music_screen_palette_changed.
    music_screen_palette_changed(context->music, from, inverted);
    ESP_LOGI(TAG, "palette %s", inverted ? "inverted" : "normal");
}

static bool apply_settings(void *context, const app_settings_t *settings) {
    settings_apply_context_t *apply_context = context;
    if (apply_context == NULL || apply_context->navigator == NULL ||
        settings == NULL || !app_settings_is_valid(settings)) {
        return false;
    }
    if (!dashboard_navigator_select_page(apply_context->navigator,
                                         (size_t)settings->selected_page)) {
        return false;
    }
    dashboard_navigator_set_units(apply_context->navigator, settings->units);
    apply_color_mode(apply_context, settings->color_mode);
    clock_screen_set_utc_offset(apply_context->clock,
                                settings->utc_offset_minutes);
    board_display_backlight_set_brightness(
        display_brightness_level(settings->brightness_percent));
    return true;
}

// État partagé par l'écran de réglages, ses actions et le timer de réglages.
// Tout s'exécute dans la tâche LVGL.
typedef struct {
    settings_runtime_t *runtime;
    settings_screen_t *screen;
    ble_window_t ble_window;
} settings_ui_t;

static settings_ui_t s_settings_ui;

static settings_ble_state_t current_ble_state(void) {
#if MGF_ENABLE_BLE_CONFIG
    if (!ble_config_service_is_started()) return SETTINGS_BLE_UNAVAILABLE;
    if (ble_config_service_is_pairing_open()) return SETTINGS_BLE_OPEN;
    return ble_config_service_phone_linked() ? SETTINGS_BLE_CONNECTED
                                             : SETTINGS_BLE_CLOSED;
#else
    return SETTINGS_BLE_UNAVAILABLE;
#endif
}

static void refresh_settings_screen(settings_ui_t *ui) {
    if (!settings_screen_is_visible(ui->screen)) return;
    app_settings_t current;
    if (settings_runtime_read(ui->runtime, &current)) {
        settings_screen_set_settings(ui->screen, &current);
    }
    settings_screen_set_bluetooth(
        ui->screen, current_ble_state(),
        ble_window_remaining_s(&ui->ble_window, lv_tick_get()));
}

static void settings_open(void *context) {
    settings_ui_t *ui = context;
    app_settings_t current;
    if (ui == NULL || !settings_runtime_read(ui->runtime, &current)) return;
    settings_screen_show(ui->screen, &current);
    refresh_settings_screen(ui);
}

static void settings_brightness_changed(void *context, uint8_t percent) {
    settings_ui_t *ui = context;
    settings_runtime_set_brightness(ui->runtime, percent);
}

static void settings_startup_page_changed(void *context,
                                          app_settings_page_t page) {
    settings_ui_t *ui = context;
    settings_runtime_set_startup_page(ui->runtime, page);
}

static void settings_color_mode_changed(void *context,
                                        app_settings_color_mode_t mode) {
    settings_ui_t *ui = context;
    settings_runtime_set_color_mode(ui->runtime, mode);
}

static void settings_bluetooth_toggled(void *context, bool open) {
    settings_ui_t *ui = context;
#if MGF_ENABLE_BLE_CONFIG
    if (open) {
        const esp_err_t error = ble_config_service_open_pairing();
        if (error == ESP_OK) {
            ble_window_open(&ui->ble_window, lv_tick_get());
        } else {
            ESP_LOGW(TAG, "cannot open BLE pairing: %s",
                     esp_err_to_name(error));
        }
    } else {
        ble_window_close(&ui->ble_window);
        (void)ble_config_service_close_pairing();
    }
#else
    (void)open;
#endif
    refresh_settings_screen(ui);
}

static void settings_timer_tick(lv_timer_t *timer) {
    settings_ui_t *ui = lv_timer_get_user_data(timer);
    settings_runtime_t *runtime = ui->runtime;
#if MGF_ENABLE_BLE_CONFIG
    app_settings_t pending;
    if (ble_config_service_take_settings_update(&pending)) {
        // Le protocole BLE v1 ne porte ni la page de démarrage, ni le fuseau,
        // ni la palette inversée : on garde les valeurs locales.
        app_settings_t current;
        if (settings_runtime_read(runtime, &current)) {
            app_settings_merge_ble_v1(&current, &pending);
        }
        settings_runtime_update(runtime, &pending);
    }
    // Le service referme lui-même la fenêtre dès qu'un téléphone est appairé.
    if (ui->ble_window.open && !ble_config_service_is_pairing_open()) {
        ble_window_close(&ui->ble_window);
    }
    if (ble_window_tick(&ui->ble_window, lv_tick_get(), false)) {
        ESP_LOGI(TAG, "BLE pairing window expired");
        (void)ble_config_service_close_pairing();
    }
#endif
    settings_runtime_process(runtime, lv_tick_get());
    refresh_settings_screen(ui);
#if MGF_ENABLE_BLE_CONFIG
    if (ble_config_service_is_started()) {
        app_settings_t current;
        if (settings_runtime_read(runtime, &current)) {
            ble_config_service_sync_settings(&current);
        }
    }
#endif
}

// Pont application compagnon -> écrans, dans la tâche LVGL. Les messages
// sont décodés par la tâche NimBLE (companion_gatt.c) ; ce timer ne fait que
// les consommer et les appliquer.
typedef struct {
    dashboard_navigator_t *navigator;
    music_screen_t *music;
    navigation_screen_t *navigation;
    settings_ui_t *settings;
    rtc_worker_t *rtc_worker;
    bool linked;
} companion_ui_t;

static companion_ui_t s_companion_ui;

#if MGF_ENABLE_BLE_CONFIG
static void apply_phone_time(companion_ui_t *ui, const companion_time_t *time) {
    const int64_t seconds = time->epoch_ms / 1000;
    rtc_datetime_t utc;
    if (!rtc_datetime_from_unix(seconds, &utc)) {
        ESP_LOGW(TAG, "phone time out of range, ignored");
        return;
    }
    // Horloge système (repli sans DS3231) et RTC, toutes deux en UTC.
    const struct timeval now = {
        .tv_sec = (time_t)seconds,
        .tv_usec = (suseconds_t)((time->epoch_ms % 1000) * 1000),
    };
    // Publier la priorité téléphone AVANT de toucher l'horloge système : le
    // worker marque phone_seen (sous verrou, avec la mise en file) puis le
    // recalage périodique RTC n'écrase plus jamais le système. Le worker reste
    // facultatif (NULL) : `false` = commande refusée (RTC absente), pas une
    // écriture échouée ; l'horloge système est quand même appliquée ci-dessous.
    if (!rtc_worker_submit(ui->rtc_worker, &utc)) {
        ESP_LOGW(TAG, "RTC not queued from phone (unavailable)");
    }
    settimeofday(&now, NULL);
    // Persisté (anti-rebond) et appliqué à l'horloge via apply_settings.
    settings_runtime_set_utc_offset(ui->settings->runtime,
                                    time->utc_offset_min);
    ESP_LOGI(TAG, "time synced from phone (UTC%+d min)", time->utc_offset_min);
}

static void apply_remote_key(companion_ui_t *ui, companion_key_t key) {
    settings_screen_t *settings = ui->settings->screen;
    if (settings_screen_is_visible(settings)) {
        // La télécommande ne pilote pas les réglages : OK/retour les ferment.
        if (key == COMPANION_KEY_BACK || key == COMPANION_KEY_OK) {
            settings_screen_hide(settings);
        }
        return;
    }
    switch (key) {
    case COMPANION_KEY_NEXT:
    case COMPANION_KEY_RIGHT:
        dashboard_navigator_next(ui->navigator);
        break;
    case COMPANION_KEY_PREVIOUS:
    case COMPANION_KEY_LEFT:
        dashboard_navigator_previous(ui->navigator);
        break;
    default:
        break;
    }
}
#endif

static void companion_timer_tick(lv_timer_t *timer) {
    companion_ui_t *ui = lv_timer_get_user_data(timer);
    const uint32_t now = lv_tick_get();
#if MGF_ENABLE_BLE_CONFIG
    const bool linked = ble_config_service_phone_linked();
    if (linked != ui->linked) {
        ui->linked = linked;
        ESP_LOGI(TAG, "companion phone %s", linked ? "linked" : "lost");
        if (!linked) music_screen_set_media(ui->music, NULL, now);
        music_screen_set_link(ui->music, linked, now);
        navigation_screen_set_link(ui->navigation, linked);
    }
    companion_media_t media;
    if (companion_gatt_take_media(&media)) {
        music_screen_set_media(ui->music, &media, now);
    }
    // Pochette : le JPEG complet part au worker (décodage hors LVGL) ; l'image
    // ambre prête est reprise ici, dans la tâche LVGL, sans jamais bloquer.
    companion_art_jpeg_t art;
    if (companion_gatt_take_art(&art)) {
        companion_art_worker_submit(&art);
    }
    uint16_t *cover = NULL;
    int cover_width = 0;
    int cover_height = 0;
    if (companion_art_worker_take(&cover, &cover_width, &cover_height)) {
        if (cover_width == MUSIC_COVER_WIDTH &&
            cover_height == MUSIC_COVER_HEIGHT) {
            // Le worker rend toujours la palette normale (forme canonique) :
            // conversion exacte vers la palette active avant remise.
            if (ui_theme_is_inverted()) {
                music_cover_convert_palette(
                    cover, (size_t)cover_width * (size_t)cover_height, false,
                    true);
            }
            music_screen_set_cover(ui->music, cover, cover_width, cover_height);
            ESP_LOGI(TAG, "cover handed to music screen: %dx%d", cover_width,
                     cover_height);
        } else {
            ESP_LOGW(TAG, "cover handoff rejected: dimensions=%dx%d",
                     cover_width, cover_height);
            companion_art_worker_free(cover);
        }
    }
    companion_nav_t nav;
    if (companion_gatt_take_nav(&nav)) {
        navigation_screen_set_route(ui->navigation, &nav);
    }
    companion_time_t phone_time;
    if (companion_gatt_take_time(&phone_time)) {
        apply_phone_time(ui, &phone_time);
    }
    companion_key_t key;
    while (companion_gatt_take_key(&key)) apply_remote_key(ui, key);
#endif
    music_screen_tick(ui->music, now);
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
    settings_screen_t *settings_screen = NULL;
    runtime_diagnostics_t *diagnostics = NULL;
    lv_timer_t *settings_timer = NULL;
    lv_timer_t *companion_timer = NULL;
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

    s_settings_apply_context = (settings_apply_context_t){
        .screen = screen,
        .navigator = navigator,
        .clock = clock_view,
        .music = music_view,
    };
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
        apply_settings, &s_settings_apply_context);
    if (settings_runtime == NULL ||
        !settings_runtime_apply_current(settings_runtime)) {
        ESP_LOGE(TAG, "could not create/apply settings runtime");
        goto cleanup;
    }
    dashboard_navigator_set_page_changed_callback(
        navigator, settings_runtime_page_changed, settings_runtime);

    // Réglages en surimpression, créés après le navigateur pour le couvrir ;
    // ouverts par un glissement vers le haut ou un maintien prolongé sur
    // n'importe quelle page.
    s_settings_ui = (settings_ui_t){.runtime = settings_runtime};
    const settings_screen_actions_t settings_actions = {
        .context = &s_settings_ui,
        .brightness_changed = settings_brightness_changed,
        .startup_page_changed = settings_startup_page_changed,
        .bluetooth_toggled = settings_bluetooth_toggled,
        .color_mode_changed = settings_color_mode_changed,
    };
    settings_screen = settings_screen_create(screen, &settings_actions);
    if (settings_screen == NULL) {
        ESP_LOGE(TAG, "could not create settings screen");
        goto cleanup;
    }
    s_settings_ui.screen = settings_screen;
    dashboard_navigator_set_settings_callback(navigator, settings_open,
                                              &s_settings_ui);

    settings_timer = lv_timer_create(settings_timer_tick, 250, &s_settings_ui);
    if (settings_timer == NULL) {
        ESP_LOGE(TAG, "could not create settings persistence timer");
        goto cleanup;
    }

    s_companion_ui = (companion_ui_t){
        .navigator = navigator,
        .music = music_view,
        .navigation = navigation_view,
        .settings = &s_settings_ui,
        .rtc_worker = rtc_worker,
    };
#if MGF_ENABLE_BLE_CONFIG
    companion_art_worker_start();
#endif
    companion_timer =
        lv_timer_create(companion_timer_tick, 200, &s_companion_ui);
    if (companion_timer == NULL) {
        ESP_LOGE(TAG, "could not create companion timer");
        goto cleanup;
    }

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
            .rtc_context = rtc_worker,
            .datetime_set = ble_datetime_set,
            .pairing_open_on_start = MGF_BLE_CONFIG_OPEN_ON_BOOT != 0,
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
    if (companion_timer != NULL) lv_timer_delete(companion_timer);
    if (settings_timer != NULL) lv_timer_delete(settings_timer);
#if MGF_ENABLE_BLE_CONFIG
    companion_art_worker_stop();
#endif
    if (settings_runtime != NULL) {
        settings_runtime_destroy(settings_runtime);
    }
    settings_persistence_destroy(settings_persistence);
#if MGF_USE_MEMS_KLINE
    if (mems_ecu != NULL) mems_ecu_destroy(mems_ecu);
#else
    if (fake_ecu != NULL) fake_ecu_destroy(fake_ecu);
#endif
    if (settings_screen != NULL) settings_screen_destroy(settings_screen);
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
