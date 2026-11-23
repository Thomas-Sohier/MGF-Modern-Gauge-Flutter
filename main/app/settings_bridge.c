#include "app/settings_bridge.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>

#include "esp_log.h"
#include "domain/ble_window.h"
#include "domain/display_brightness.h"
#include "domain/music_cover.h"
#include "infrastructure/board_display.h"
#include "ui/themes/ui_theme.h"

#if MGF_ENABLE_BLE_CONFIG
#include "domain/rtc_time.h"
#include "infrastructure/ble_config_service.h"
#endif

static const char *TAG = "mgf_gauge";

// État partagé par l'application des réglages, l'écran de réglages, ses
// actions et le timer de réglages. Tout s'exécute dans la tâche LVGL.
struct settings_bridge_s {
    lv_obj_t *root;
    dashboard_navigator_t *navigator;
    clock_screen_t *clock;
    music_screen_t *music;
    settings_runtime_t *runtime;
    settings_screen_t *screen;
    lv_timer_t *timer;
    ble_window_t ble_window;
#if MGF_ENABLE_BLE_CONFIG
    app_settings_t ble_synced;
    bool has_ble_synced;
#endif
};

static settings_bridge_t s_bridge;

settings_bridge_t *
settings_bridge_create(const settings_bridge_config_t *config) {
    if (config == NULL) return NULL;
    s_bridge = (settings_bridge_t){
        .root = config->screen,
        .navigator = config->navigator,
        .clock = config->clock,
        .music = config->music,
    };
    return &s_bridge;
}

// Palette normale/inversée appliquée instantanément, sans recréer les écrans
// (média, pochette, itinéraire et page courante sont conservés) : les styles
// locaux sont remappés rôle par rôle et les callbacks de dessin relisent la
// palette active. No-op si la palette est déjà la bonne.
static void apply_color_mode(settings_bridge_t *bridge,
                             app_settings_color_mode_t mode) {
    const bool inverted = mode == APP_SETTINGS_COLORS_INVERTED;
    const bool from = ui_theme_is_inverted();
    if (!ui_theme_apply_inverted(bridge->root, inverted)) return;
    // La pochette affichée appartient à l'écran musique : ses pixels sont
    // convertis sans perte (music_cover_convert_palette) par
    // music_screen_palette_changed.
    music_screen_palette_changed(bridge->music, from, inverted);
    ESP_LOGI(TAG, "palette %s", inverted ? "inverted" : "normal");
}

bool settings_bridge_apply(void *context, const app_settings_t *settings) {
    settings_bridge_t *bridge = context;
    if (bridge == NULL || bridge->navigator == NULL || settings == NULL ||
        !app_settings_is_valid(settings)) {
        return false;
    }
    if (!dashboard_navigator_select_page(bridge->navigator,
                                         (size_t)settings->selected_page)) {
        return false;
    }
    dashboard_navigator_set_units(bridge->navigator, settings->units);
    apply_color_mode(bridge, settings->color_mode);
    clock_screen_set_utc_offset(bridge->clock, settings->utc_offset_minutes);
    board_display_backlight_set_brightness(
        display_brightness_level(settings->brightness_percent));
#if MGF_ENABLE_BLE_CONFIG
    if (ble_config_service_is_started()) {
        if (ble_config_service_sync_settings(settings) == ESP_OK) {
            bridge->ble_synced = *settings;
            bridge->has_ble_synced = true;
        }
    }
#endif
    return true;
}

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

static void refresh_settings_screen(settings_bridge_t *bridge) {
    if (!settings_screen_is_visible(bridge->screen)) return;
    app_settings_t current;
    if (settings_runtime_read(bridge->runtime, &current)) {
        settings_screen_set_settings(bridge->screen, &current);
    }
    settings_screen_set_bluetooth(
        bridge->screen, current_ble_state(),
        ble_window_remaining_s(&bridge->ble_window, lv_tick_get()));
}

static void settings_open(void *context) {
    settings_bridge_t *bridge = context;
    app_settings_t current;
    if (bridge == NULL || !settings_runtime_read(bridge->runtime, &current)) {
        return;
    }
    settings_screen_show(bridge->screen, &current);
    refresh_settings_screen(bridge);
}

static void settings_brightness_changed(void *context, uint8_t percent) {
    settings_bridge_t *bridge = context;
    settings_runtime_set_brightness(bridge->runtime, percent);
}

static void settings_startup_page_changed(void *context,
                                          app_settings_page_t page) {
    settings_bridge_t *bridge = context;
    settings_runtime_set_startup_page(bridge->runtime, page);
}

static void settings_color_mode_changed(void *context,
                                        app_settings_color_mode_t mode) {
    settings_bridge_t *bridge = context;
    settings_runtime_set_color_mode(bridge->runtime, mode);
}

static void settings_bluetooth_toggled(void *context, bool open) {
    settings_bridge_t *bridge = context;
#if MGF_ENABLE_BLE_CONFIG
    if (open) {
        const esp_err_t error = ble_config_service_open_pairing();
        if (error == ESP_OK) {
            ble_window_open(&bridge->ble_window, lv_tick_get());
        } else {
            ESP_LOGW(TAG, "cannot open BLE pairing: %s",
                     esp_err_to_name(error));
        }
    } else {
        ble_window_close(&bridge->ble_window);
        (void)ble_config_service_close_pairing();
    }
#else
    (void)open;
#endif
    refresh_settings_screen(bridge);
}

static void settings_timer_tick(lv_timer_t *timer) {
    settings_bridge_t *bridge = lv_timer_get_user_data(timer);
    settings_runtime_t *runtime = bridge->runtime;
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
    if (bridge->ble_window.open && !ble_config_service_is_pairing_open()) {
        ble_window_close(&bridge->ble_window);
    }
    // Retry : sans ce filet, un échec transitoire de ble_gap_adv_start au boot
    // laissait la jauge invisible jusqu'au redémarrage suivant.
    ble_config_service_ensure_advertising();
    // Une connexion en cours (appairage inclus) repousse l'échéance : la
    // fenêtre ne coupe pas un téléphone en train de se lier à 5:00 pile.
    if (ble_window_tick(&bridge->ble_window, lv_tick_get(),
                        ble_config_service_has_connection())) {
        ESP_LOGI(TAG, "BLE pairing window expired");
        (void)ble_config_service_close_pairing();
    }
#endif
    settings_runtime_process(runtime, lv_tick_get());
    refresh_settings_screen(bridge);
#if MGF_ENABLE_BLE_CONFIG
    if (ble_config_service_is_started()) {
        app_settings_t current;
        if (settings_runtime_read(runtime, &current) &&
            (!bridge->has_ble_synced ||
             !app_settings_equal(&current, &bridge->ble_synced))) {
            if (ble_config_service_sync_settings(&current) == ESP_OK) {
                bridge->ble_synced = current;
                bridge->has_ble_synced = true;
            }
        }
    }
#endif
}

bool settings_bridge_start(settings_bridge_t *bridge, lv_obj_t *parent,
                           settings_runtime_t *runtime) {
    if (bridge == NULL) return false;
    bridge->runtime = runtime;
    // Réglages en surimpression, créés après le navigateur pour le couvrir ;
    // ouverts par un glissement vers le haut ou un maintien prolongé sur
    // n'importe quelle page.
    const settings_screen_actions_t actions = {
        .context = bridge,
        .brightness_changed = settings_brightness_changed,
        .startup_page_changed = settings_startup_page_changed,
        .bluetooth_toggled = settings_bluetooth_toggled,
        .color_mode_changed = settings_color_mode_changed,
    };
    bridge->screen = settings_screen_create(parent, &actions);
    if (bridge->screen == NULL) {
        ESP_LOGE(TAG, "could not create settings screen");
        return false;
    }
    dashboard_navigator_set_settings_callback(bridge->navigator, settings_open,
                                              bridge);

    bridge->timer = lv_timer_create(settings_timer_tick, 250, bridge);
    if (bridge->timer == NULL) {
        ESP_LOGE(TAG, "could not create settings persistence timer");
        return false;
    }
    return true;
}

settings_screen_t *settings_bridge_screen(const settings_bridge_t *bridge) {
    return bridge != NULL ? bridge->screen : NULL;
}

void settings_bridge_stop(settings_bridge_t *bridge) {
    if (bridge == NULL || bridge->timer == NULL) return;
    lv_timer_delete(bridge->timer);
    bridge->timer = NULL;
}

void settings_bridge_destroy(settings_bridge_t *bridge) {
    if (bridge == NULL || bridge->screen == NULL) return;
    settings_screen_destroy(bridge->screen);
    bridge->screen = NULL;
}

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

esp_err_t settings_bridge_start_ble(settings_runtime_t *runtime,
                                    rtc_worker_t *rtc_worker,
                                    bool pairing_open_on_start) {
    const ble_config_service_config_t ble_config = {
        .device_name = "MGF Gauge",
        .settings_context = runtime,
        .settings_read = settings_runtime_read,
        .settings_update = ble_settings_accept,
        .rtc_context = rtc_worker,
        .datetime_set = ble_datetime_set,
        .pairing_open_on_start = pairing_open_on_start,
    };
    return ble_config_service_start(&ble_config);
}
#endif
