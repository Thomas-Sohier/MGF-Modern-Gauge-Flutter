#include "app/companion_bridge.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_task_wdt.h"
#include "lvgl.h"

#include "domain/companion_protocol.h"

#ifndef MGF_ENABLE_BLE_CONFIG
#define MGF_ENABLE_BLE_CONFIG 0
#endif

#if MGF_ENABLE_BLE_CONFIG
#include "domain/music_cover.h"
#include "domain/rtc_time.h"
#include "infrastructure/ble_config_service.h"
#include "infrastructure/companion_art_worker.h"
#include "infrastructure/companion_gatt.h"
#include "ui/themes/ui_theme.h"
#endif

static const char *TAG = "mgf_gauge";

typedef struct {
    companion_bridge_config_t config;
    lv_timer_t *timer;
    bool linked;
    // Dernier art_id annoncé par le média courant : sert à jeter une pochette
    // d'un morceau déjà passé (zapping) avant même de la décoder.
    char current_art_id[COMPANION_MEDIA_ART_ID_MAX];
} companion_bridge_t;

static companion_bridge_t s_bridge;

#if MGF_ENABLE_BLE_CONFIG
static void apply_phone_time(companion_bridge_t *bridge,
                             const companion_time_t *time) {
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
    if (!rtc_worker_submit(bridge->config.rtc_worker, &utc)) {
        ESP_LOGW(TAG, "RTC not queued from phone (unavailable)");
    }
    settimeofday(&now, NULL);
    // Persisté (anti-rebond) et appliqué à l'horloge via settings_bridge_apply.
    settings_runtime_set_utc_offset(bridge->config.settings_runtime,
                                    time->utc_offset_min);
    ESP_LOGI(TAG, "time synced from phone (UTC%+d min)", time->utc_offset_min);
}

static void apply_remote_key(companion_bridge_t *bridge, companion_key_t key) {
    settings_screen_t *settings = bridge->config.settings_screen;
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
        dashboard_navigator_next(bridge->config.navigator);
        break;
    case COMPANION_KEY_PREVIOUS:
    case COMPANION_KEY_LEFT:
        dashboard_navigator_previous(bridge->config.navigator);
        break;
    default:
        break;
    }
}
#endif

// Diagnostic du gel d'écran : la tâche LVGL s'abonne au watchdog de tâches et
// le nourrit à chaque passage de ce timer (200 ms). Si elle reste bloquée plus
// de CONFIG_ESP_TASK_WDT_TIMEOUT_S, le panic écrit un core dump en flash.
static void lvgl_watchdog_feed(void) {
    static bool subscribed;
    if (!subscribed) {
        const esp_err_t err = esp_task_wdt_add(NULL);
        subscribed = err == ESP_OK || err == ESP_ERR_INVALID_ARG;
        ESP_LOGI(TAG, "LVGL task watchdog: %s", esp_err_to_name(err));
    }
    if (subscribed) esp_task_wdt_reset();
}

static void companion_timer_tick(lv_timer_t *timer) {
    companion_bridge_t *bridge = lv_timer_get_user_data(timer);
    lvgl_watchdog_feed();
    music_screen_t *music = bridge->config.music;
    const uint32_t now = lv_tick_get();
#if MGF_ENABLE_BLE_CONFIG
    navigation_screen_t *navigation = bridge->config.navigation;
    // Expire tout réassemblage de pochette bloqué (transfert interrompu) :
    // sans cela le prochain contrôle …0003 serait refusé indéfiniment.
    companion_gatt_tick_art_timeout();
    const bool linked = ble_config_service_phone_linked();
    if (linked != bridge->linked) {
        bridge->linked = linked;
        ESP_LOGI(TAG, "companion phone %s", linked ? "linked" : "lost");
        if (!linked) {
            bridge->current_art_id[0] = '\0';
            music_screen_set_media(music, NULL, now);
            music_screen_set_link(music, false, now);
            navigation_screen_set_link(navigation, false);
        } else {
            music_screen_set_link(music, true, now);
            navigation_screen_set_link(navigation, true);
        }
    }
    if (!linked) {
        // Déconnecté : on purge les messages orphelins déjà en boîte aux lettres
        // pour qu'ils ne réactivent jamais l'UI après la perte du téléphone.
        companion_art_jpeg_t stale_art;
        if (companion_gatt_take_art(&stale_art)) {
            companion_gatt_free_art(stale_art.data);
        }
        uint16_t *stale_cover = NULL;
        int stale_width = 0;
        int stale_height = 0;
        if (companion_art_worker_take_with_id(&stale_cover, &stale_width,
                                              &stale_height, NULL, 0)) {
            companion_art_worker_free(stale_cover);
        }
        companion_media_t stale_media;
        (void)companion_gatt_take_media(&stale_media);
        companion_nav_t stale_nav;
        (void)companion_gatt_take_nav(&stale_nav);
        companion_key_t stale_key;
        while (companion_gatt_take_key(&stale_key)) {
        }
        music_screen_tick(music, now);
        return;
    }
    companion_media_t media;
    if (companion_gatt_take_media(&media)) {
        // Mémorisé AVANT le rendu : c'est la référence pour juger les pochettes
        // qui arrivent ensuite (une art_id différente est périmée).
        strncpy(bridge->current_art_id, media.art_id,
                sizeof(bridge->current_art_id) - 1U);
        bridge->current_art_id[sizeof(bridge->current_art_id) - 1U] = '\0';
        music_screen_set_media(music, &media, now);
    }
    // Pochette : le JPEG complet part au worker (décodage hors LVGL) ; l'image
    // ambre prête est reprise ici, dans la tâche LVGL, sans jamais bloquer.
    companion_art_jpeg_t art;
    if (companion_gatt_take_art(&art)) {
        // Pochette d'un morceau déjà passé (zapping pendant le transfert) :
        // inutile de la décoder, elle serait rejetée à la remise.
        if (bridge->current_art_id[0] != '\0' && art.art_id[0] != '\0' &&
            strcmp(art.art_id, bridge->current_art_id) != 0) {
            companion_gatt_free_art(art.data);
        } else {
            companion_art_worker_submit(&art);
        }
    }
    uint16_t *cover = NULL;
    int cover_width = 0;
    int cover_height = 0;
    char cover_art_id[COMPANION_ART_ID_MAX] = {0};
    if (companion_art_worker_take_with_id(&cover, &cover_width, &cover_height,
                                          cover_art_id, sizeof(cover_art_id))) {
        // Le décodage JPEG a pu finir après un changement de morceau : ne
        // jamais écraser la nouvelle pochette avec l'ancienne.
        if (bridge->current_art_id[0] != '\0' && cover_art_id[0] != '\0' &&
            strcmp(cover_art_id, bridge->current_art_id) != 0) {
            ESP_LOGI(TAG, "stale cover dropped (art_id mismatch)");
            companion_art_worker_free(cover);
        } else if (cover_width == MUSIC_COVER_WIDTH &&
                   cover_height == MUSIC_COVER_HEIGHT) {
            // Le worker rend toujours la palette normale (forme canonique) :
            // conversion exacte vers la palette active avant remise.
            if (ui_theme_is_inverted()) {
                music_cover_convert_palette(
                    cover, (size_t)cover_width * (size_t)cover_height, false,
                    true);
            }
            music_screen_set_cover(music, cover, cover_width, cover_height);
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
        navigation_screen_set_route(navigation, &nav);
    }
    companion_time_t phone_time;
    if (companion_gatt_take_time(&phone_time)) {
        apply_phone_time(bridge, &phone_time);
    }
    companion_key_t key;
    while (companion_gatt_take_key(&key)) apply_remote_key(bridge, key);
#endif
    music_screen_tick(music, now);
}

bool companion_bridge_start(const companion_bridge_config_t *config) {
    if (config == NULL) return false;
    s_bridge = (companion_bridge_t){.config = *config};
#if MGF_ENABLE_BLE_CONFIG
    companion_art_worker_start();
#endif
    // 80 ms (12,5 Hz) : cadence demandée par la rotation 3D de la cassette de
    // repli de l'écran musique ; les reprises BLE/pochette/télécommande sont
    // toutes non bloquantes, ce rythme reste très léger pour l'ESP32-S3.
    s_bridge.timer = lv_timer_create(companion_timer_tick, 80, &s_bridge);
    if (s_bridge.timer == NULL) {
        ESP_LOGE(TAG, "could not create companion timer");
        return false;
    }
    return true;
}

void companion_bridge_destroy(void) {
    if (s_bridge.timer != NULL) {
        lv_timer_delete(s_bridge.timer);
        s_bridge.timer = NULL;
    }
#if MGF_ENABLE_BLE_CONFIG
    companion_art_worker_stop();
#endif
}
