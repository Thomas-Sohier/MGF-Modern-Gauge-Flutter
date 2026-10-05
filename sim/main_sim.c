// Harnais de simulation hôte : compile la même UI LVGL que la cible ESP32-S3,
// rend une frame offscreen avec des données ECU mock et exporte le rendu en PNG
// (golden test).
//
// Usage : gen_golden <style> <chemin_png> [scenario] [palette]
//
//   style    : boot | amber | clock | music | navigation | faults | temps |
//              injection | lambda | ignition | idle | admission | dashboard |
//              settings
//   scenario : standard (défaut) | offline | missing | hot | sensor_fault |
//              imperial | long | paused | unsynced | reconnected
//
//   palette  : normal (défaut) | inverted (palette inversée dès la création)
//              | toggled (créé en normal puis basculé à chaud par
//              ui_theme_apply_inverted : doit être identique à « inverted »)
//              | roundtrip (normal -> inversé -> normal à chaud : doit être
//              identique au golden normal, aucune dérive)
//
// Un scénario ou une palette inconnus sont refusés avec un code de retour 2.

#include "lvgl.h"
#include "ui/screens/boot_screen.h"
#include "ui/screens/style_amber.h"
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
#include "domain/app_settings.h"
#include "domain/ecu_data.h"
#include "domain/companion_protocol.h"
#include "domain/music_cover.h"
#include "infrastructure/jpeg_cover.h"
#include "music_cover_fixture.h"
#include "ui/ui_layout.h"

#ifndef TTF_PATH
#define TTF_PATH "../main/fonts/Michroma-Regular.ttf"
#endif

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Buffer partiel dimensionné pour la plus grande largeur possible.
#define BUF_W 1024

// ── Scénarios ────────────────────────────────────────────────────────────────
typedef enum {
    SCENARIO_STANDARD = 0,
    SCENARIO_OFFLINE,      // ECU déconnectée : toutes les mesures à NAN
    SCENARIO_MISSING,      // connectée mais eau/huile indisponibles
    SCENARIO_HOT,          // surchauffe : eau 110 °C, huile 135 °C
    SCENARIO_SENSOR_FAULT, // sonde d'eau en défaut (drapeau + mesure NAN)
    SCENARIO_IMPERIAL,     // unités impériales sur les vues qui les gèrent
    SCENARIO_LONG,         // textes compagnon longs (ellipsis)
    SCENARIO_PAUSED,       // lecture média en pause
    SCENARIO_UNSYNCED,     // horloge sans source de temps fiable
    SCENARIO_RECONNECTED,  // valid -> invalid -> valid (dernier état nominal)
} scenario_t;

static bool scenario_parse(const char *name, scenario_t *out) {
    if (name == NULL || out == NULL) return false;
    static const struct {
        const char *name;
        scenario_t scenario;
    } k_scenarios[] = {
        {"standard", SCENARIO_STANDARD},
        {"offline", SCENARIO_OFFLINE},
        {"missing", SCENARIO_MISSING},
        {"hot", SCENARIO_HOT},
        {"sensor_fault", SCENARIO_SENSOR_FAULT},
        {"imperial", SCENARIO_IMPERIAL},
        {"long", SCENARIO_LONG},
        {"paused", SCENARIO_PAUSED},
        {"unsynced", SCENARIO_UNSYNCED},
        {"reconnected", SCENARIO_RECONNECTED},
    };
    for (size_t i = 0; i < sizeof(k_scenarios) / sizeof(k_scenarios[0]); i++) {
        if (strcmp(k_scenarios[i].name, name) == 0) {
            *out = k_scenarios[i].scenario;
            return true;
        }
    }
    return false;
}

// ── Instantanés ECU mock ─────────────────────────────────────────────────────
// Vue nominale : moteur chaud au ralenti, tous les capteurs présents.
static ecu_data_t online_snapshot(void) {
    return (ecu_data_t){
        .connected = true,
        .faults_available = true,
        .fault_flags = ECU_FAULT_INTAKE_AIR_SENSOR,
        .rpm = 875,
        .throttle = 7,
        .coolant_temp = 89,
        .battery_voltage = 14.2f,
        .oil_temp = 96,
        .ambient_temp = 22,
        .intake_air_temp = 31,
        .fuel_rail_temp = 38,
        .map_sensor_kpa = 34,
        .throttle_pot_voltage = 0.72f,
        .ignition_advance = 14.5f,
        .ignition_advance_offset = -1.2f,
        .coil_1_charge_time = 2.45f,
        .coil_2_charge_time = 2.52f,
        .coil_time_microseconds = 2480,
        .injector_1_pw = 2.18f,
        .injector_2_pw = 2.24f,
        .fuelling_feedback_percent = 101,
        .short_term_trim_percent = 2.8f,
        .long_term_trim = -1.7f,
        .lambda_mv = 680,
        .o2_mv = 665,
        .estimated_air_fuel = 14.65f,
        .lambda_sensor_duty_cycle = 53,
        .idle_setpoint = 850,
        .idle_adjuster_rpm = 18,
        .idle_error = 25,
        .idle_valve_position = 34,
        .idle_base_position = 30,
    };
}

// Instantané dérivé du scénario demandé (l'unité reste portée par la vue via
// set_units ; le scénario imperial ne change donc pas les valeurs Celsius).
static ecu_data_t scenario_mock(scenario_t scenario) {
    switch (scenario) {
    case SCENARIO_OFFLINE:
        return ecu_data_unavailable();
    case SCENARIO_MISSING: {
        ecu_data_t data = online_snapshot();
        data.coolant_temp = NAN;
        data.oil_temp = NAN;
        return data;
    }
    case SCENARIO_HOT: {
        ecu_data_t data = online_snapshot();
        data.coolant_temp = 110.0f;
        data.oil_temp = 135.0f;
        return data;
    }
    case SCENARIO_SENSOR_FAULT: {
        ecu_data_t data = online_snapshot();
        data.coolant_temp = NAN;
        data.faults_available = true;
        data.fault_flags = ECU_FAULT_COOLANT_SENSOR;
        return data;
    }
    default:
        return online_snapshot();
    }
}

// ── Charges utiles de l'application compagnon ────────────────────────────────
static const char *k_navigation_json =
    "{\"active\":true,\"instruction\":\"Tournez à droite sur Rue des Lilas\","
    "\"distance\":\"300 m\",\"eta\":\"Arrivée 14:32\"}";

// Consigne volontairement > 80 caractères, avec accents français : la voie
// dépasse la largeur du libellé et exerce l'ellipsis (LV_LABEL_LONG_DOT).
static const char *k_navigation_long_json =
    "{\"active\":true,\"instruction\":\"Tournez à droite sur Avenue des "
    "Champs-Élysées puis continuez tout droit jusqu'au rond-point de la "
    "Concorde\",\"distance\":\"1,2 km\",\"eta\":\"Arrivée 14:32\"}";

static const char *music_json_for(scenario_t scenario) {
    switch (scenario) {
    case SCENARIO_LONG:
        // Titre et artiste > 80 caractères, accentués : ellipsis du libellé.
        return "{\"title\":\"Balade nocturne sur les routes de montagne entre "
               "amis et paysages étoilés magnifiques\",\"artist\":\"Compilation "
               "française des années quatre-vingt et quatre-vingt-dix\","
               "\"state\":\"playing\",\"position_ms\":142000,"
               "\"duration_ms\":228000}";
    case SCENARIO_PAUSED:
        return "{\"title\":\"Let It Happen\",\"artist\":\"Tame Impala\","
               "\"state\":\"paused\",\"position_ms\":142000,"
               "\"duration_ms\":466000}";
    default:
        return "{\"title\":\"Let It Happen\",\"artist\":\"Tame Impala\","
               "\"state\":\"playing\",\"position_ms\":142000,"
               "\"duration_ms\":466000}";
    }
}

// ── Adaptateurs de pages du tableau de bord ──────────────────────────────────
// Mêmes signatures que la table de production (app_main.c) : création,
// actualisation ECU, destruction.
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
DEFINE_PAGE_ADAPTER(amber)
DEFINE_PAGE_ADAPTER(faults)
DEFINE_PAGE_ADAPTER(temps)
DEFINE_PAGE_ADAPTER(injection)
DEFINE_PAGE_ADAPTER(lambda)
DEFINE_PAGE_ADAPTER(ignition)
DEFINE_PAGE_ADAPTER(idle)
DEFINE_PAGE_ADAPTER(admission)

// Musique et navigation sont événementielles : pas de mise à jour ECU.
static void *music_page_create(lv_obj_t *parent) {
    return music_screen_create(parent);
}

static void music_page_destroy(void *context) {
    music_screen_destroy(context);
}

static void *navigation_page_create(lv_obj_t *parent) {
    return navigation_screen_create(parent);
}

static void navigation_page_destroy(void *context) {
    navigation_screen_destroy(context);
}

static void amber_page_settings(void *context, app_settings_units_t units) {
    amber_screen_set_units(context, units);
}

static void temps_page_settings(void *context, app_settings_units_t units) {
    temps_screen_set_units(context, units);
}

static void admission_page_settings(void *context, app_settings_units_t units) {
    admission_screen_set_units(context, units);
}

typedef void *(*sim_page_create_t)(lv_obj_t *parent);

// Crée une page, sa vue puis l'enregistre ; toute allocation manquante est
// signalée et la vue déjà créée est détruite.
static bool dashboard_add_page(dashboard_navigator_t *navigator,
                               const char *name, sim_page_create_t create,
                               dashboard_page_update_cb_t update,
                               dashboard_page_destroy_cb_t destroy,
                               dashboard_page_settings_cb_t settings_changed,
                               uint32_t update_period_ms, void **out_context) {
    lv_obj_t *page = dashboard_navigator_create_page(navigator);
    void *view = page != NULL ? create(page) : NULL;
    const dashboard_page_t descriptor = {
        .name = name,
        .context = view,
        .update = update,
        .destroy = destroy,
        .settings_changed = settings_changed,
        .update_period_ms = update_period_ms,
    };
    if (page == NULL || view == NULL ||
        !dashboard_navigator_register_page(navigator, page, &descriptor)) {
        fprintf(stderr, "dashboard: page %s indisponible\n", name);
        if (view != NULL && destroy != NULL) destroy(view);
        return false;
    }
    if (out_context != NULL) *out_context = view;
    return true;
}

// Construit les 11 pages dans l'ordre de production, actualise chacune puis
// sélectionne la page attendue par le scénario. Le tableau de bord est laissé
// en place sur `screen` (pas de destruction : le processus s'arrête après le
// rendu, comme les autres styles).
static int dashboard_build(lv_obj_t *screen, scenario_t scenario) {
    dashboard_navigator_t *navigator = dashboard_navigator_create(screen);
    if (navigator == NULL) {
        fprintf(stderr, "dashboard: allocation du navigateur echouee\n");
        return 1;
    }

    void *contexts[11] = {0};
    const bool built =
        dashboard_add_page(navigator, "HEURE", clock_page_create,
                           clock_page_update, clock_page_destroy, NULL, 1000,
                           &contexts[0]) &&
        dashboard_add_page(navigator, "MUSIQUE", music_page_create, NULL,
                           music_page_destroy, NULL, 0, &contexts[1]) &&
        dashboard_add_page(navigator, "NAVIGATION", navigation_page_create,
                           NULL, navigation_page_destroy, NULL, 0,
                           &contexts[2]) &&
        dashboard_add_page(navigator, "RPM", amber_page_create,
                           amber_page_update, amber_page_destroy,
                           amber_page_settings, 40, &contexts[3]) &&
        dashboard_add_page(navigator, "DEFAUTS", faults_page_create,
                           faults_page_update, faults_page_destroy, NULL, 1000,
                           &contexts[4]) &&
        dashboard_add_page(navigator, "TEMPERATURES", temps_page_create,
                           temps_page_update, temps_page_destroy,
                           temps_page_settings, 150, &contexts[5]) &&
        dashboard_add_page(navigator, "INJECTION", injection_page_create,
                           injection_page_update, injection_page_destroy, NULL,
                           80, &contexts[6]) &&
        dashboard_add_page(navigator, "LAMBDA", lambda_page_create,
                           lambda_page_update, lambda_page_destroy, NULL, 80,
                           &contexts[7]) &&
        dashboard_add_page(navigator, "ALLUMAGE", ignition_page_create,
                           ignition_page_update, ignition_page_destroy, NULL,
                           80, &contexts[8]) &&
        dashboard_add_page(navigator, "RALENTI", idle_page_create,
                           idle_page_update, idle_page_destroy, NULL, 150,
                           &contexts[9]) &&
        dashboard_add_page(navigator, "ADMISSION", admission_page_create,
                           admission_page_update, admission_page_destroy,
                           admission_page_settings, 150, &contexts[10]);
    if (!built || dashboard_navigator_count(navigator) != 11) {
        fprintf(stderr, "dashboard: pages incompletes (%zu/11)\n",
                dashboard_navigator_count(navigator));
        dashboard_navigator_destroy(navigator);
        return 1;
    }

    const ecu_data_t mock = scenario_mock(scenario);
    // Amorce l'instantané puis force la mise à jour de chaque page (couverture
    // des 11 callbacks, pas seulement la page visible).
    dashboard_navigator_update(navigator, &mock);
    for (size_t i = 0; i < dashboard_navigator_count(navigator); i++) {
        if (!dashboard_navigator_select_page(navigator, i)) {
            fprintf(stderr, "dashboard: selection page %zu refusee\n", i);
            dashboard_navigator_destroy(navigator);
            return 1;
        }
    }

    if (scenario == SCENARIO_HOT) {
        // NAVIGATION (index 2) est événementielle : aucun callback ECU. Le
        // bandeau thermique est posé par le navigateur lui-même ; l'itinéraire
        // réel vérifie qu'il ne masque pas la manœuvre.
        if (!dashboard_navigator_select_page(navigator, 2)) {
            fprintf(stderr, "dashboard: selection NAVIGATION refusee\n");
            dashboard_navigator_destroy(navigator);
            return 1;
        }
        companion_nav_t route;
        if (!companion_parse_nav(k_navigation_json, strlen(k_navigation_json),
                                 &route)) {
            fprintf(stderr,
                    "dashboard: itineraire de demonstration invalide\n");
            dashboard_navigator_destroy(navigator);
            return 1;
        }
        navigation_screen_set_link(contexts[2], true);
        navigation_screen_set_route(contexts[2], &route);
        dashboard_navigator_update(navigator, &mock);
    } else if (!dashboard_navigator_select_page(navigator, 6)) {
        // Page par défaut du golden : INJECTION.
        fprintf(stderr, "dashboard: selection INJECTION refusee\n");
        dashboard_navigator_destroy(navigator);
        return 1;
    }
    return 0;
}

// ── Tick LVGL basé sur l'horloge monotone ────────────────────────────────────
static uint32_t tick_cb(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ── Display offscreen (flush no-op) ──────────────────────────────────────────
static uint8_t draw_buf[BUF_W * 80 * 4];

static void flush_cb(lv_display_t *disp, const lv_area_t *area,
                     uint8_t *px_map) {
    LV_UNUSED(area);
    LV_UNUSED(px_map);
    lv_display_flush_ready(disp);
}

// ── Export ARGB8888 -> PNG RGB via stb ───────────────────────────────────────
// Masque les pixels hors du disque inscrit (panneau physiquement rond
// de la LILYGO T-RGB H597) -> golden représentatif de l'écran réel.
static int write_png(const lv_draw_buf_t *snap, const char *path) {
    const uint32_t w = snap->header.w;
    const uint32_t h = snap->header.h;
    const uint32_t stride = snap->header.stride;
    uint8_t *rgb = malloc((size_t)w * h * 3);
    if (!rgb) return 0;

    const float cx = (w - 1) * 0.5f, cy = (h - 1) * 0.5f;
    const float r = (float)((w < h ? w : h)) * 0.5f;
    const float r2 = r * r;

    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *row = snap->data + (size_t)y * stride;
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *p =
                row + (size_t)x * 4; // ARGB8888 little-endian : B,G,R,A
            uint8_t *o = rgb + ((size_t)y * w + x) * 3;
            const float dx = (float)x - cx, dy = (float)y - cy;
            if (dx * dx + dy * dy > r2) { // hors du disque : bezel noir
                o[0] = o[1] = o[2] = 0;
            } else {
                o[0] = p[2]; // R
                o[1] = p[1]; // G
                o[2] = p[0]; // B
            }
        }
    }
    const int ok = stbi_write_png(path, (int)w, (int)h, 3, rgb, (int)w * 3);
    free(rgb);
    return ok;
}

// Charge un fichier entier en mémoire (malloc, non libéré).
static void *load_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    if (buf) *out_size = (size_t)n;
    return buf;
}

int main(int argc, char **argv) {
    // Usage : gen_golden <style> <png> [scenario] (scénario inconnu -> exit 2).
    const char *style = (argc > 1) ? argv[1] : "amber";
    const char *out = (argc > 2) ? argv[2] : "rpm_amber.png";

    scenario_t scenario = SCENARIO_STANDARD;
    if (argc > 3 && !scenario_parse(argv[3], &scenario)) {
        fprintf(stderr, "scenario inconnu : %s\n", argv[3]);
        return 2;
    }
    const char *palette = (argc > 4) ? argv[4] : "normal";
    const bool palette_inverted = strcmp(palette, "inverted") == 0;
    const bool palette_toggled = strcmp(palette, "toggled") == 0;
    const bool palette_roundtrip = strcmp(palette, "roundtrip") == 0;
    if (!palette_inverted && !palette_toggled && !palette_roundtrip &&
        strcmp(palette, "normal") != 0) {
        fprintf(stderr, "palette inconnue : %s\n", palette);
        return 2;
    }
    ui_theme_set_inverted(palette_inverted);
    settings_screen_t *settings_view = NULL;
    music_screen_t *music_view = NULL;

    // Cible LILYGO T-RGB H597 : écran IPS ROND 480x480 (driver ST7701S,
    // interface RGB). On rend à la résolution réelle, avec masque circulaire.
    const int32_t W = UI_DISPLAY_SIZE_PX;
    const int32_t H = UI_DISPLAY_SIZE_PX;

    lv_init();
    lv_tick_set_cb(tick_cb);

    // Police Michroma (sinon repli Montserrat).
    size_t ttf_size = 0;
    void *ttf = load_file(TTF_PATH, &ttf_size);
    if (ttf)
        ui_fonts_init(ttf, ttf_size);
    else
        fprintf(stderr, "warn: TTF introuvable (%s), repli Montserrat\n",
                TTF_PATH);

    lv_display_t *disp = lv_display_create(W, H);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    const ecu_data_t mock = scenario_mock(scenario);
    const ecu_data_t offline = ecu_data_unavailable();

    if (strcmp(style, "boot") == 0) {
        if (boot_screen_create(screen) == NULL) {
            fprintf(stderr, "boot: allocation echouee\n");
            return 1;
        }
    } else if (strcmp(style, "amber") == 0) {
        amber_screen_t *view = amber_screen_create(screen);
        if (view == NULL) {
            fprintf(stderr, "amber: allocation echouee\n");
            return 1;
        }
        if (scenario == SCENARIO_IMPERIAL)
            amber_screen_set_units(view, APP_SETTINGS_UNITS_IMPERIAL);
        if (scenario == SCENARIO_RECONNECTED) {
            // ECU connectée -> déconnectée -> connectée : la transition est
            // exercée, le rendu reste l'état nominal.
            amber_screen_update(view, &mock);
            amber_screen_update(view, &offline);
            amber_screen_update(view, &mock);
        } else {
            amber_screen_update(view, &mock);
        }
    } else if (strcmp(style, "clock") == 0) {
        clock_screen_t *view = clock_screen_create(screen);
        if (view == NULL) {
            fprintf(stderr, "clock: allocation echouee\n");
            return 1;
        }
        clock_screen_update(view, &mock);
        if (scenario == SCENARIO_OFFLINE || scenario == SCENARIO_UNSYNCED) {
            // Forcé APRÈS la mise à jour : aucune source de temps fiable.
            clock_screen_set_time(view, 0, 0, false);
        } else if (scenario == SCENARIO_RECONNECTED) {
            // valid -> invalid -> valid via l'API, dernier état nominal.
            clock_screen_set_time(view, 10, 10, true);
            clock_screen_set_time(view, 0, 0, false);
            clock_screen_set_time(view, 10, 10, true);
        }
    } else if (strcmp(style, "music") == 0) {
        music_screen_t *view = music_screen_create(screen);
        music_view = view;
        if (view == NULL) {
            fprintf(stderr, "music: allocation echouee\n");
            return 1;
        }
        if (scenario != SCENARIO_OFFLINE) {
            companion_media_t media;
            const char *json = music_json_for(scenario);
            if (!companion_parse_media(json, strlen(json), &media)) {
                fprintf(stderr, "music: media de demonstration invalide\n");
                return 1;
            }
            music_screen_set_link(view, true, 0);
            music_screen_set_media(view, &media, 0);
        }
        // Le simulateur n'a pas de BLE : la pochette de test est décodée ici
        // (même décodeur tjpgd que la cible, hors boucle LVGL) pour que le
        // golden vérifie la vraie image monochrome et son cadrage « cover ».
        if (scenario != SCENARIO_OFFLINE) {
            uint16_t *cover = malloc((size_t)MUSIC_COVER_WIDTH *
                                     MUSIC_COVER_HEIGHT * sizeof(uint16_t));
            if (cover != NULL &&
                jpeg_cover_decode_amber(
                    k_music_cover_jpeg, k_music_cover_jpeg_len,
                    MUSIC_COVER_WIDTH, MUSIC_COVER_HEIGHT, cover)) {
                // Même remise que app_main.c : rendu canonique normal,
                // converti vers la palette active.
                if (ui_theme_is_inverted()) {
                    music_cover_convert_palette(
                        cover, (size_t)MUSIC_COVER_WIDTH * MUSIC_COVER_HEIGHT,
                        false, true);
                }
                music_screen_set_cover(view, cover, MUSIC_COVER_WIDTH,
                                       MUSIC_COVER_HEIGHT);
            } else {
                free(cover);
            }
        }
    } else if (strcmp(style, "navigation") == 0) {
        navigation_screen_t *view = navigation_screen_create(screen);
        if (view == NULL) {
            fprintf(stderr, "navigation: allocation echouee\n");
            return 1;
        }
        if (scenario != SCENARIO_OFFLINE) {
            const char *json = scenario == SCENARIO_LONG
                                   ? k_navigation_long_json
                                   : k_navigation_json;
            companion_nav_t nav;
            if (!companion_parse_nav(json, strlen(json), &nav)) {
                fprintf(stderr, "navigation: itineraire invalide\n");
                return 1;
            }
            navigation_screen_set_link(view, true);
            navigation_screen_set_route(view, &nav);
        }
    } else if (strcmp(style, "faults") == 0) {
        faults_screen_t *view = faults_screen_create(screen);
        if (view == NULL) {
            fprintf(stderr, "faults: allocation echouee\n");
            return 1;
        }
        faults_screen_update(view, &mock);
    } else if (strcmp(style, "temps") == 0) {
        temps_screen_t *view = temps_screen_create(screen);
        if (view == NULL) {
            fprintf(stderr, "temps: allocation echouee\n");
            return 1;
        }
        if (scenario == SCENARIO_IMPERIAL)
            temps_screen_set_units(view, APP_SETTINGS_UNITS_IMPERIAL);
        temps_screen_update(view, &mock);
    } else if (strcmp(style, "injection") == 0) {
        injection_screen_t *view = injection_screen_create(screen);
        if (view == NULL) {
            fprintf(stderr, "injection: allocation echouee\n");
            return 1;
        }
        injection_screen_update(view, &mock);
    } else if (strcmp(style, "lambda") == 0) {
        lambda_screen_t *view = lambda_screen_create(screen);
        if (view == NULL) {
            fprintf(stderr, "lambda: allocation echouee\n");
            return 1;
        }
        lambda_screen_update(view, &mock);
    } else if (strcmp(style, "ignition") == 0) {
        ignition_screen_t *view = ignition_screen_create(screen);
        if (view == NULL) {
            fprintf(stderr, "ignition: allocation echouee\n");
            return 1;
        }
        ignition_screen_update(view, &mock);
    } else if (strcmp(style, "idle") == 0) {
        idle_screen_t *view = idle_screen_create(screen);
        if (view == NULL) {
            fprintf(stderr, "idle: allocation echouee\n");
            return 1;
        }
        idle_screen_update(view, &mock);
    } else if (strcmp(style, "admission") == 0) {
        admission_screen_t *view = admission_screen_create(screen);
        if (view == NULL) {
            fprintf(stderr, "admission: allocation echouee\n");
            return 1;
        }
        if (scenario == SCENARIO_IMPERIAL)
            admission_screen_set_units(view, APP_SETTINGS_UNITS_IMPERIAL);
        admission_screen_update(view, &mock);
    } else if (strcmp(style, "dashboard") == 0) {
        if (dashboard_build(screen, scenario) != 0) return 1;
    } else if (strcmp(style, "settings") == 0) {
        // Réglages ouverts : luminosité ~60 %, démarrage sur le compte-tours,
        // fenêtre BLE ouverte (ou image sans BLE en mode « offline »).
        const settings_screen_actions_t actions = {0};
        settings_screen_t *settings = settings_screen_create(screen, &actions);
        if (settings == NULL) {
            fprintf(stderr, "settings: allocation echouee\n");
            return 1;
        }
        app_settings_t values;
        app_settings_defaults(&values);
        values.brightness_percent = 60;
        values.startup_page = APP_SETTINGS_PAGE_RPM;
        if (palette_inverted) values.color_mode = APP_SETTINGS_COLORS_INVERTED;
        settings_screen_show(settings, &values);
        settings_view = settings;
        settings_screen_set_bluetooth(settings,
                                      scenario == SCENARIO_OFFLINE
                                          ? SETTINGS_BLE_UNAVAILABLE
                                          : SETTINGS_BLE_OPEN,
                                      272);
    } else {
        fprintf(stderr, "style inconnu : %s\n", style);
        return 2;
    }

    // Bascules à chaud comme sur la cible (apply_color_mode dans app_main.c),
    // chacune précédée d'un rendu complet dans la palette courante.
    const int toggles = palette_toggled ? 1 : (palette_roundtrip ? 2 : 0);
    for (int toggle = 0; toggle < toggles; toggle++) {
        lv_obj_update_layout(screen);
        lv_refr_now(disp);
        const bool from_inverted = ui_theme_is_inverted();
        const bool inverted = !from_inverted;
        if (!ui_theme_apply_inverted(screen, inverted)) {
            fprintf(stderr, "bascule de palette refusee\n");
            return 1;
        }
        music_screen_palette_changed(music_view, from_inverted, inverted);
        if (settings_view != NULL) {
            app_settings_t values;
            app_settings_defaults(&values);
            values.brightness_percent = 60;
            values.startup_page = APP_SETTINGS_PAGE_RPM;
            values.color_mode = inverted ? APP_SETTINGS_COLORS_INVERTED
                                         : APP_SETTINGS_COLORS_NORMAL;
            settings_screen_set_settings(settings_view, &values);
        }
    }

    // Résout la disposition puis force un cycle de rendu.
    lv_obj_update_layout(screen);
    lv_refr_now(disp);

    lv_draw_buf_t *snap = lv_snapshot_take(screen, LV_COLOR_FORMAT_ARGB8888);
    if (!snap) {
        fprintf(stderr, "lv_snapshot_take a echoue\n");
        return 1;
    }

    if (!write_png(snap, out)) {
        fprintf(stderr, "ecriture PNG echouee: %s\n", out);
        lv_draw_buf_destroy(snap);
        return 1;
    }

    printf("golden ecrit: %s (%ux%u)\n", out, snap->header.w, snap->header.h);
    lv_draw_buf_destroy(snap);
    return 0;
}
