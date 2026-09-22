#include "ui/screens/temps_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>

// Repère logique 320 px, identique aux autres écrans ambre. La composition
// reste volontairement dans la zone sûre du disque : l'eau occupe le premier
// plan et l'huile/l'air forment deux jauges secondaires, sans grille.
#define SCREEN_CX          160.0f
#define HEADER_Y            23.0f

#define HERO_CY             96.0f
#define HERO_R_OUT          59.0f
#define HERO_R_IN           48.0f
#define HERO_START         205.0f
#define HERO_END           335.0f
#define HERO_NAME_Y        143.0f
#define HERO_UNIT_X        160.0f
#define HERO_UNIT_Y        121.0f
#define HERO_SCALE          288

#define CARD_CY            207.0f
#define CARD_R_OUT          41.0f
#define CARD_R_IN           33.0f
#define CARD_START        205.0f
#define CARD_END          335.0f
#define CARD_LEFT           91.0f
#define CARD_RIGHT         229.0f
#define CARD_VALUE_Y       207.0f
#define CARD_UNIT_Y        233.0f
#define CARD_NAME_Y        259.0f

#define STATUS_Y           280.0f
#define SUMMARY_Y          298.0f
#define METRIC_COUNT         3

enum {
    METRIC_COOLANT = 0,
    METRIC_OIL,
    METRIC_INTAKE,
};

typedef struct {
    const char *name;
    float maximum;
    float danger;
} metric_def_t;

static const metric_def_t kMetrics[METRIC_COUNT] = {
    {"EAU",   150.0f, 105.0f},
    {"HUILE", 160.0f, 130.0f},
    {"AIR",    80.0f,   0.0f},
};

struct temps_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;
    lv_obj_t *overlay;

    lv_obj_t *header;
    lv_obj_t *hero_value;
    lv_obj_t *hero_unit;
    lv_obj_t *hero_name;
    lv_obj_t *status;
    lv_obj_t *summary;
    lv_obj_t *metric_value[METRIC_COUNT];
    lv_obj_t *metric_unit[METRIC_COUNT];
    lv_obj_t *metric_name[METRIC_COUNT];

    char hero_text[16];
    char metric_text[METRIC_COUNT][16];
    char status_text[24];
    char summary_text[24];

    float values[METRIC_COUNT];
    bool available[METRIC_COUNT];
    bool danger[METRIC_COUNT];
    bool connected;
    bool has_snapshot;
    int32_t displayed[METRIC_COUNT];
    int32_t arc_bucket[METRIC_COUNT];
    int32_t peak_display;
    bool any_danger_display;
    app_settings_units_t units;
    ecu_data_t latest_data;
    bool has_latest_data;
};

static void draw_arc(lv_layer_t *layer, int32_t cx, int32_t cy,
                     int32_t radius, int32_t width, float start, float end,
                     lv_color_t color) {
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.center.x = cx;
    dsc.center.y = cy;
    dsc.radius = radius;
    dsc.width = width;
    dsc.start_angle = start;
    dsc.end_angle = end;
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.rounded = 0;
    lv_draw_arc(layer, &dsc);
}

static float temperature_display(float celsius, app_settings_units_t units) {
    return units == APP_SETTINGS_UNITS_IMPERIAL
               ? celsius * 9.0f / 5.0f + 32.0f : celsius;
}

static const char *temperature_unit(app_settings_units_t units) {
    return units == APP_SETTINGS_UNITS_IMPERIAL ? "°F" : "°C";
}

static float metric_max_display(int index, app_settings_units_t units) {
    return temperature_display(kMetrics[index].maximum, units);
}

static float clamp_progress(float value, float maximum) {
    if (!isfinite(value) || maximum <= 0.0f) return 0.0f;
    if (value <= 0.0f) return 0.0f;
    if (value >= maximum) return 1.0f;
    return value / maximum;
}

static void draw_progress_arc(lv_layer_t *layer, float cx, float cy,
                              float radius, float thickness, float start,
                              float end, float progress, lv_color_t base,
                              lv_color_t active) {
    const int32_t px = (int32_t)lroundf(cx);
    const int32_t py = (int32_t)lroundf(cy);
    const int32_t pr = (int32_t)lroundf(radius);
    const int32_t pw = (int32_t)lroundf(thickness);

    draw_arc(layer, px, py, pr, pw, start, end, base);
    if (progress > 0.0f) {
        draw_arc(layer, px, py, pr, pw, start,
                 start + (end - start) * progress, active);
    }
}

static void canvas_draw_cb(lv_event_t *event) {
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    temps_screen_t *scr = lv_obj_get_user_data(canvas);
    if (scr == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = ui_layout_fit((float)lv_area_get_width(&area),
                                             (float)lv_area_get_height(&area));
    const float ox = (float)area.x1 + layout.ox;
    const float oy = (float)area.y1 + layout.oy;
    const float k = layout.scale;
    const float cx = ox + SCREEN_CX * k;
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t bright = ui_theme_amber_bright();

    // Une jauge dominante pour l'eau : grande réserve ambre, sans aiguilles,
    // graduations ni séparateurs qui réduiraient la zone de lecture.
    const float coolant_progress = clamp_progress(
        scr->values[METRIC_COOLANT],
        metric_max_display(METRIC_COOLANT, scr->units));
    draw_progress_arc(layer, cx, oy + HERO_CY * k, HERO_R_OUT * k,
                      (HERO_R_OUT - HERO_R_IN) * k, HERO_START, HERO_END,
                      coolant_progress, dim, bright);

    // Deux jauges principales secondaires. Elles sont assez espacées pour
    // garder leurs valeurs et unités lisibles sur la dalle ronde.
    const float oil_progress = clamp_progress(
        scr->values[METRIC_OIL], metric_max_display(METRIC_OIL, scr->units));
    draw_progress_arc(layer, ox + CARD_LEFT * k, oy + CARD_CY * k,
                      CARD_R_OUT * k, (CARD_R_OUT - CARD_R_IN) * k,
                      CARD_START, CARD_END, oil_progress, dim, bright);

    const float intake_progress = clamp_progress(
        scr->values[METRIC_INTAKE],
        metric_max_display(METRIC_INTAKE, scr->units));
    draw_progress_arc(layer, ox + CARD_RIGHT * k, oy + CARD_CY * k,
                      CARD_R_OUT * k, (CARD_R_OUT - CARD_R_IN) * k,
                      CARD_START, CARD_END, intake_progress, dim, bright);
}

static void place_labels(temps_screen_t *scr) {
    amber_ui_place_centered(scr->header, scr->root, SCREEN_CX, HEADER_Y,
                             0.0f, 0.0f);
    amber_ui_place_centered(scr->hero_value, scr->root, SCREEN_CX, HERO_CY,
                             0.0f, 0.0f);
    amber_ui_place_centered(scr->hero_unit, scr->root, HERO_UNIT_X,
                             HERO_UNIT_Y, 0.0f, 0.0f);
    amber_ui_place_centered(scr->hero_name, scr->root, SCREEN_CX,
                             HERO_NAME_Y, 0.0f, 0.0f);
    amber_ui_place_centered(scr->status, scr->root, SCREEN_CX, STATUS_Y,
                             0.0f, 0.0f);
    amber_ui_place_centered(scr->summary, scr->root, SCREEN_CX, SUMMARY_Y,
                             0.0f, 0.0f);

    amber_ui_place_centered(scr->metric_value[METRIC_OIL], scr->root,
                             CARD_LEFT, CARD_VALUE_Y, 0.0f, 0.0f);
    amber_ui_place_centered(scr->metric_unit[METRIC_OIL], scr->root,
                             CARD_LEFT, CARD_UNIT_Y, 0.0f, 0.0f);
    amber_ui_place_centered(scr->metric_name[METRIC_OIL], scr->root,
                             CARD_LEFT, CARD_NAME_Y, 0.0f, 0.0f);
    amber_ui_place_centered(scr->metric_value[METRIC_INTAKE], scr->root,
                             CARD_RIGHT, CARD_VALUE_Y, 0.0f, 0.0f);
    amber_ui_place_centered(scr->metric_unit[METRIC_INTAKE], scr->root,
                             CARD_RIGHT, CARD_UNIT_Y, 0.0f, 0.0f);
    amber_ui_place_centered(scr->metric_name[METRIC_INTAKE], scr->root,
                             CARD_RIGHT, CARD_NAME_Y, 0.0f, 0.0f);
}

temps_screen_t *temps_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    temps_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    scr->root = amber_ui_root_create(parent);
    if (scr->root == NULL) goto fail;
    scr->canvas = amber_ui_canvas_create(scr->root, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    scr->overlay = lv_obj_create(scr->root);
    if (scr->overlay == NULL) goto fail;
    lv_obj_remove_style_all(scr->overlay);
    lv_obj_set_size(scr->overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(scr->overlay, LV_OBJ_FLAG_SCROLLABLE);

    snprintf(scr->hero_text, sizeof(scr->hero_text), "--");
    snprintf(scr->status_text, sizeof(scr->status_text), "PAS DE LIAISON");
    snprintf(scr->summary_text, sizeof(scr->summary_text), "EN ATTENTE");
    for (int i = 0; i < METRIC_COUNT; i++) {
        snprintf(scr->metric_text[i], sizeof(scr->metric_text[i]), "--");
    }

    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_font_t *large = amber_ui_font_hero();
    const lv_font_t *caption = amber_ui_font_caption();

    scr->header = amber_ui_label_create(scr->overlay, caption, bright,
                                        "TEMPERATURES", 0.0f);
    scr->hero_value = amber_ui_label_create(scr->overlay, large, bright,
                                            scr->hero_text, 0.0f);
    scr->hero_unit = amber_ui_label_create(
        scr->overlay, caption, bright, temperature_unit(scr->units), 0.0f);
    scr->hero_name = amber_ui_label_create(scr->overlay, caption, bright, "EAU",
                                           0.0f);
    scr->status = amber_ui_label_create(scr->overlay, caption, dim,
                                        scr->status_text, 0.0f);
    scr->summary = amber_ui_label_create(scr->overlay, caption, dim,
                                         scr->summary_text, 0.0f);

    if (scr->header == NULL || scr->hero_value == NULL ||
        scr->hero_unit == NULL || scr->hero_name == NULL ||
        scr->status == NULL || scr->summary == NULL) {
        goto fail;
    }

    // L'eau garde la plus grande valeur visuelle, sans changer la police
    // Michroma partagée : le zoom LVGL reste centré et scalable avec le repère.
    lv_obj_set_style_transform_scale(scr->hero_value, HERO_SCALE, 0);

    for (int i = METRIC_OIL; i <= METRIC_INTAKE; i++) {
        scr->metric_value[i] = amber_ui_label_create(
            scr->overlay, large, dim, scr->metric_text[i], 0.0f);
        scr->metric_unit[i] = amber_ui_label_create(
            scr->overlay, caption, dim, temperature_unit(scr->units), 0.0f);
        scr->metric_name[i] = amber_ui_label_create(
            scr->overlay, caption, bright, kMetrics[i].name, 0.0f);
        if (scr->metric_value[i] == NULL || scr->metric_unit[i] == NULL ||
            scr->metric_name[i] == NULL) {
            goto fail;
        }
    }

    lv_obj_update_layout(scr->root);
    place_labels(scr);
    return scr;

fail:
    temps_screen_destroy(scr);
    return NULL;
}

void temps_screen_set_units(temps_screen_t *scr, app_settings_units_t units) {
    if (scr == NULL || units >= APP_SETTINGS_UNITS_COUNT || scr->units == units) {
        return;
    }
    scr->units = units;
    lv_label_set_text_static(scr->hero_unit, temperature_unit(units));
    for (int i = METRIC_OIL; i <= METRIC_INTAKE; i++) {
        lv_label_set_text_static(scr->metric_unit[i], temperature_unit(units));
    }
    scr->has_snapshot = false;
    if (scr->has_latest_data) temps_screen_update(scr, &scr->latest_data);
}

void temps_screen_update(temps_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL || scr->canvas == NULL) return;
    scr->latest_data = *data;
    scr->has_latest_data = true;

    const float raw_values[METRIC_COUNT] = {
        data->coolant_temp,
        data->oil_temp,
        data->intake_air_temp,
    };
    int32_t next_displayed[METRIC_COUNT];
    int32_t next_arc_bucket[METRIC_COUNT];
    bool next_available[METRIC_COUNT];
    bool next_danger[METRIC_COUNT];
    float peak = 0.0f;
    int32_t peak_display = 0;
    bool any_danger = false;

    for (int i = 0; i < METRIC_COUNT; i++) {
        const float raw = raw_values[i];
        const float display = temperature_display(raw, scr->units);
        next_available[i] = data->connected && isfinite(raw);
        next_displayed[i] = next_available[i]
            ? (int32_t)lroundf(display) : INT32_MIN;
        next_arc_bucket[i] = next_available[i]
            ? (int32_t)lroundf(clamp_progress(
                  display, metric_max_display(i, scr->units)) * 1000.0f)
            : 0;
        next_danger[i] = next_available[i] && kMetrics[i].danger > 0.0f &&
                         raw >= kMetrics[i].danger;
        any_danger = any_danger || next_danger[i];
        if (next_available[i] && display > peak) peak = display;
    }
    peak_display = (int32_t)lroundf(peak);

    if (scr->has_snapshot && scr->peak_display == peak_display &&
        scr->connected == data->connected) {
        bool same = true;
        for (int i = 0; i < METRIC_COUNT; i++) {
            same = same && scr->displayed[i] == next_displayed[i] &&
                   scr->arc_bucket[i] == next_arc_bucket[i] &&
                   scr->available[i] == next_available[i] &&
                   scr->danger[i] == next_danger[i];
        }
        if (same) return;
    }

    const bool arc_changed = !scr->has_snapshot ||
        scr->arc_bucket[METRIC_COOLANT] != next_arc_bucket[METRIC_COOLANT] ||
        scr->arc_bucket[METRIC_OIL] != next_arc_bucket[METRIC_OIL] ||
        scr->arc_bucket[METRIC_INTAKE] != next_arc_bucket[METRIC_INTAKE];
    const bool hero_changed = !scr->has_snapshot ||
        scr->displayed[METRIC_COOLANT] != next_displayed[METRIC_COOLANT];
    bool text_changed = false;
    for (int i = 0; i < METRIC_COUNT; i++) {
        const float raw = raw_values[i];
        const float display = temperature_display(raw, scr->units);
        const bool metric_changed = !scr->has_snapshot ||
            scr->available[i] != next_available[i] ||
            scr->displayed[i] != next_displayed[i];
        scr->available[i] = next_available[i];
        scr->values[i] = scr->available[i] ? display : 0.0f;
        scr->danger[i] = next_danger[i];
        scr->displayed[i] = next_displayed[i];
        scr->arc_bucket[i] = next_arc_bucket[i];
        any_danger = any_danger || scr->danger[i];
        if (scr->available[i] && display > peak) peak = display;

        if (metric_changed) {
            if (scr->available[i]) {
                snprintf(scr->metric_text[i], sizeof(scr->metric_text[i]),
                         "%.0f", display);
            } else {
                snprintf(scr->metric_text[i], sizeof(scr->metric_text[i]), "--");
            }
            text_changed = true;
            if (i != METRIC_COOLANT) {
                lv_label_set_text_static(scr->metric_value[i], scr->metric_text[i]);
                lv_obj_set_style_text_color(
                    scr->metric_value[i], scr->available[i]
                        ? ui_theme_amber_bright() : ui_theme_amber_dim(), 0);
                lv_obj_set_style_text_color(
                    scr->metric_unit[i], scr->available[i]
                        ? ui_theme_amber_bright() : ui_theme_amber_dim(), 0);
            }
        }
    }

    if (hero_changed) text_changed = true;
    if (scr->available[METRIC_COOLANT]) {
        snprintf(scr->hero_text, sizeof(scr->hero_text), "%.0f",
                 scr->values[METRIC_COOLANT]);
    } else {
        snprintf(scr->hero_text, sizeof(scr->hero_text), "--");
    }
    if (hero_changed) lv_label_set_text_static(scr->hero_value, scr->hero_text);

    const bool status_changed = !scr->has_snapshot ||
        scr->connected != data->connected ||
        scr->peak_display != peak_display ||
        scr->any_danger_display != any_danger;
    if (!status_changed) goto skip_status_text;

    if (!data->connected) {
        snprintf(scr->status_text, sizeof(scr->status_text), "PAS DE LIAISON");
        snprintf(scr->summary_text, sizeof(scr->summary_text), "EN ATTENTE");
    } else if (any_danger) {
        snprintf(scr->status_text, sizeof(scr->status_text), "ALERTE THERMIQUE");
        snprintf(scr->summary_text, sizeof(scr->summary_text), "MAX. %.0f %s",
                 peak, temperature_unit(scr->units));
    } else {
        snprintf(scr->status_text, sizeof(scr->status_text), "TEMPERATURES OK");
        snprintf(scr->summary_text, sizeof(scr->summary_text), "MAX. %.0f %s",
                 peak, temperature_unit(scr->units));
    }
    lv_label_set_text_static(scr->status, scr->status_text);
    lv_label_set_text_static(scr->summary, scr->summary_text);
    lv_obj_set_style_text_color(scr->status, ui_theme_amber_bright(), 0);
    lv_obj_set_style_text_color(scr->summary, ui_theme_amber_dim(), 0);
    text_changed = true;

skip_status_text:
    scr->peak_display = peak_display;
    scr->connected = data->connected;
    scr->any_danger_display = any_danger;
    scr->has_snapshot = true;

    // Les labels à largeur intrinsèque changent de largeur avec une valeur à
    // trois chiffres : les recentrer ne crée aucun objet supplémentaire.
    if (text_changed) place_labels(scr);
    if (arc_changed) lv_obj_invalidate(scr->canvas);
}

void temps_screen_destroy(temps_screen_t *scr) {
    if (scr == NULL) return;
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
