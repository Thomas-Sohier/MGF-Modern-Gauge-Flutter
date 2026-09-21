#include "ui/screens/temps_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>

// ── Repère logique : 320 px, identique à style_amber.c ─────────────────────
#define SCREEN_CX       160.0f
#define HEADER_Y         20.0f
#define HEADER_LINE_Y     43.0f

#define HERO_CY         114.0f
#define HERO_R_OUT       58.0f
#define HERO_R_IN        51.0f
#define HERO_START     208.0f
#define HERO_END       332.0f
#define HERO_TICK_R0    62.0f
#define HERO_TICK_R1    67.0f

// Les cinq colonnes sont rentrées dans le disque : à 480 px, les deux
// colonnes extrêmes restent ainsi lisibles jusque dans leurs libellés.
#define MATRIX_LINE_Y   184.0f
#define CELL_CY         214.0f
#define CELL_R_OUT       19.0f
#define CELL_R_IN        15.0f
#define CELL_START     210.0f
#define CELL_END       330.0f
#define MATRIX_BOTTOM_Y 278.0f
#define STATUS_LINE_Y   285.0f
#define STATUS_Y        292.0f
#define SUMMARY_Y       306.0f
#define METRIC_X0        58.0f
#define METRIC_STEP      51.0f
#define METRIC_VALUE_Y  243.0f
#define METRIC_NAME_Y   259.0f

#define SEPARATOR_W      0.8f
#define METRIC_COUNT       5

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum {
    METRIC_COOLANT = 0,
    METRIC_OIL,
    METRIC_INTAKE,
    METRIC_AMBIENT,
    METRIC_FUEL,
};

typedef struct {
    const char *name;
    float maximum;
    float danger;
} metric_def_t;

static const metric_def_t kMetrics[METRIC_COUNT] = {
    {"EAU",      150.0f, 105.0f},
    {"HUILE",    160.0f, 130.0f},
    {"AIR",       80.0f,   0.0f},
    {"AMB.",      60.0f,   0.0f},
    {"CARB.",    120.0f,  90.0f},
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
    lv_obj_t *metric_name[METRIC_COUNT];

    char hero_text[16];
    char metric_text[METRIC_COUNT][16];
    char status_text[24];
    char summary_text[24];

    float values[METRIC_COUNT];
    bool available[METRIC_COUNT];
    bool danger[METRIC_COUNT];
};

static void draw_line(lv_layer_t *layer, float x1, float y1, float x2,
                      float y2, float width, lv_color_t color) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.width = LV_MAX(1, (int32_t)lroundf(width));
    dsc.round_start = 0;
    dsc.round_end = 0;
    dsc.p1.x = (int32_t)lroundf(x1);
    dsc.p1.y = (int32_t)lroundf(y1);
    dsc.p2.x = (int32_t)lroundf(x2);
    dsc.p2.y = (int32_t)lroundf(y2);
    lv_draw_line(layer, &dsc);
}

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

static float clamp_progress(float value, float maximum) {
    if (!isfinite(value) || maximum <= 0.0f) return 0.0f;
    if (value <= 0.0f) return 0.0f;
    if (value >= maximum) return 1.0f;
    return value / maximum;
}

static void polar_point(float cx, float cy, float radius, float degrees,
                        float *x, float *y) {
    const float radians = degrees * (float)M_PI / 180.0f;
    *x = cx + cosf(radians) * radius;
    *y = cy + sinf(radians) * radius;
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

static void draw_separator_with_gap(lv_layer_t *layer, float x, float y0,
                                    float y1, float gap, float scale) {
    draw_line(layer, x, y0, x, y0 + (y1 - y0) * 0.5f - gap,
              SEPARATOR_W * scale, ui_theme_amber_separator());
    draw_line(layer, x, y0 + (y1 - y0) * 0.5f + gap, x, y1,
              SEPARATOR_W * scale, ui_theme_amber_separator());
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
    const float hero_cy = oy + HERO_CY * k;
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t separator = ui_theme_amber_separator();

    // Fine ligne d'en-tête, interrompue au centre comme la grille du cadran.
    draw_line(layer, ox + 53.0f * k, oy + HEADER_LINE_Y * k,
              ox + 132.0f * k, oy + HEADER_LINE_Y * k,
              SEPARATOR_W * k, separator);
    draw_line(layer, ox + 188.0f * k, oy + HEADER_LINE_Y * k,
              ox + 267.0f * k, oy + HEADER_LINE_Y * k,
              SEPARATOR_W * k, separator);

    // Anneau principal : une réserve dim et une progression ambre active.
    const float hero_progress = clamp_progress(scr->values[METRIC_COOLANT],
                                               kMetrics[METRIC_COOLANT].maximum);
    draw_progress_arc(layer, cx, hero_cy, HERO_R_OUT * k,
                      (HERO_R_OUT - HERO_R_IN) * k, HERO_START, HERO_END,
                      hero_progress, dim, bright);

    // Index radiaux discrets autour de l'anneau, tous dans la palette ambre.
    for (int i = 0; i <= 6; i++) {
        const float angle = HERO_START + (HERO_END - HERO_START) * (float)i / 6.0f;
        float x0, y0, x1, y1;
        polar_point(cx, hero_cy, HERO_TICK_R0 * k, angle, &x0, &y0);
        polar_point(cx, hero_cy, HERO_TICK_R1 * k, angle, &x1, &y1);
        draw_line(layer, x0, y0, x1, y1, SEPARATOR_W * k, separator);
    }

    // Petit repère central : pas d'aiguille, seulement une lecture optique.
    draw_line(layer, cx - 8.0f * k, hero_cy, cx + 8.0f * k, hero_cy,
              SEPARATOR_W * k, separator);
    draw_line(layer, cx, hero_cy - 8.0f * k, cx, hero_cy + 8.0f * k,
              SEPARATOR_W * k, separator);

    // La matrice basse conserve une séparation ouverte à chaque intersection.
    draw_line(layer, ox + 35.0f * k, oy + MATRIX_LINE_Y * k,
              ox + 285.0f * k, oy + MATRIX_LINE_Y * k,
              SEPARATOR_W * k, separator);
    for (int i = 1; i < METRIC_COUNT; i++) {
        const float x = ox + (METRIC_X0 + METRIC_STEP * ((float)i - 0.5f)) * k;
        draw_separator_with_gap(layer, x, oy + MATRIX_LINE_Y * k,
                                oy + MATRIX_BOTTOM_Y * k, 7.0f * k, k);
    }

    // Cinq jauges thermiques compactes : même geste visuel, cinq plages ECU.
    for (int i = 0; i < METRIC_COUNT; i++) {
        const float x = ox + (METRIC_X0 + (float)i * METRIC_STEP) * k;
        const float progress = clamp_progress(scr->values[i],
                                               kMetrics[i].maximum);
        const lv_color_t active = (i == METRIC_COOLANT || scr->danger[i])
                                      ? bright : dim;
        draw_progress_arc(layer, x, oy + CELL_CY * k, CELL_R_OUT * k,
                          (CELL_R_OUT - CELL_R_IN) * k, CELL_START, CELL_END,
                          progress, dim, active);
    }

    draw_line(layer, ox + 62.0f * k, oy + STATUS_LINE_Y * k,
              ox + 258.0f * k, oy + STATUS_LINE_Y * k,
              SEPARATOR_W * k, separator);
}

static void place_labels(temps_screen_t *scr) {
    amber_ui_place_centered(scr->header, scr->root, SCREEN_CX, HEADER_Y,
                             0.0f, 0.0f);
    amber_ui_place_centered(scr->hero_value, scr->root, SCREEN_CX,
                             HERO_CY - 4.0f, 0.0f, 0.0f);
    amber_ui_place_centered(scr->hero_unit, scr->root, SCREEN_CX + 43.0f,
                             HERO_CY + 16.0f, 0.0f, 0.0f);
    amber_ui_place_centered(scr->hero_name, scr->root, SCREEN_CX, 157.0f,
                             0.0f, 0.0f);
    amber_ui_place_centered(scr->status, scr->root, SCREEN_CX, STATUS_Y,
                             0.0f, 0.0f);
    amber_ui_place_centered(scr->summary, scr->root, SCREEN_CX, SUMMARY_Y,
                             0.0f, 0.0f);

    for (int i = 0; i < METRIC_COUNT; i++) {
        const float x = METRIC_X0 + (float)i * METRIC_STEP;
        amber_ui_place_centered(scr->metric_value[i], scr->root, x,
                                METRIC_VALUE_Y, 0.0f, 0.0f);
        amber_ui_place_centered(scr->metric_name[i], scr->root, x,
                                METRIC_NAME_Y, 0.0f, 0.0f);
    }
}

static const lv_font_t *value_font(void) {
    return ui_font_or(ui_font_l, &lv_font_montserrat_20);
}

static const lv_font_t *small_font(void) {
    return &lv_font_montserrat_14;
}

temps_screen_t *temps_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    temps_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    scr->root = amber_ui_root_create(parent);
    if (scr->root == NULL) goto fail;
    // This screen historically used a square root; keep its background and
    // clipping behavior unchanged while sharing the common construction.
    lv_obj_set_style_radius(scr->root, 0, 0);
    lv_obj_set_style_clip_corner(scr->root, false, 0);
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
    const lv_font_t *value = value_font();
    const lv_font_t *small = small_font();

    scr->header = amber_ui_label_create(scr->overlay, small, bright,
                                        "TEMPERATURES / °C", 0.0f);
    scr->hero_value = amber_ui_label_create(
        scr->overlay, ui_font_or(ui_font_xl, &lv_font_montserrat_48), bright,
        scr->hero_text, 0.0f);
    scr->hero_unit = amber_ui_label_create(scr->overlay, value, bright, "°C",
                                           0.0f);
    scr->hero_name = amber_ui_label_create(scr->overlay, small, bright, "EAU",
                                           0.0f);
    scr->status = amber_ui_label_create(scr->overlay, small, dim,
                                        scr->status_text, 0.0f);
    scr->summary = amber_ui_label_create(scr->overlay, small, dim,
                                         scr->summary_text, 0.0f);

    if (scr->header == NULL || scr->hero_value == NULL ||
        scr->hero_unit == NULL ||
        scr->hero_name == NULL || scr->status == NULL || scr->summary == NULL) {
        goto fail;
    }

    for (int i = 0; i < METRIC_COUNT; i++) {
        scr->metric_value[i] = amber_ui_label_create(
            scr->overlay, value, dim, scr->metric_text[i], 0.0f);
        scr->metric_name[i] = amber_ui_label_create(
            scr->overlay, small, dim, kMetrics[i].name, 0.0f);
        if (scr->metric_value[i] == NULL || scr->metric_name[i] == NULL) {
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

void temps_screen_update(temps_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL || scr->canvas == NULL) return;

    float peak = 0.0f;
    bool any_danger = false;
    for (int i = 0; i < METRIC_COUNT; i++) {
        const float raw = (i == METRIC_COOLANT) ? data->coolant_temp
                         : (i == METRIC_OIL) ? data->oil_temp
                         : (i == METRIC_INTAKE) ? data->intake_air_temp
                         : (i == METRIC_AMBIENT) ? data->ambient_temp
                         : data->fuel_rail_temp;
        scr->available[i] = data->connected && isfinite(raw);
        scr->values[i] = scr->available[i] ? raw : 0.0f;
        scr->danger[i] = scr->available[i] && kMetrics[i].danger > 0.0f &&
                         raw >= kMetrics[i].danger;
        any_danger = any_danger || scr->danger[i];
        if (scr->available[i] && raw > peak) peak = raw;

        if (scr->available[i]) {
            // L'unité est annoncée dans l'en-tête : garder les cinq nombres
            // seuls évite qu'un suffixe « °C » fasse toucher deux colonnes.
            snprintf(scr->metric_text[i], sizeof(scr->metric_text[i]),
                     "%.0f", raw);
        } else {
            snprintf(scr->metric_text[i], sizeof(scr->metric_text[i]), "--");
        }
        lv_label_set_text_static(scr->metric_value[i], scr->metric_text[i]);
        lv_obj_set_style_text_color(scr->metric_value[i],
                                    scr->danger[i] || i == METRIC_COOLANT
                                        ? ui_theme_amber_bright()
                                        : ui_theme_amber_dim(), 0);
        lv_obj_set_style_text_color(scr->metric_name[i],
                                    scr->danger[i] || i == METRIC_COOLANT
                                        ? ui_theme_amber_bright()
                                        : ui_theme_amber_dim(), 0);
    }

    if (scr->available[METRIC_COOLANT]) {
        snprintf(scr->hero_text, sizeof(scr->hero_text), "%.0f",
                 scr->values[METRIC_COOLANT]);
    } else {
        snprintf(scr->hero_text, sizeof(scr->hero_text), "--");
    }
    lv_label_set_text_static(scr->hero_value, scr->hero_text);

    if (!data->connected) {
        snprintf(scr->status_text, sizeof(scr->status_text), "PAS DE LIAISON");
        snprintf(scr->summary_text, sizeof(scr->summary_text), "EN ATTENTE");
    } else if (any_danger) {
        snprintf(scr->status_text, sizeof(scr->status_text), "ALERTE THERMIQUE");
        snprintf(scr->summary_text, sizeof(scr->summary_text), "MAX. %.0f °C", peak);
    } else {
        snprintf(scr->status_text, sizeof(scr->status_text), "TEMPERATURES OK");
        snprintf(scr->summary_text, sizeof(scr->summary_text), "MAX. %.0f °C", peak);
    }
    lv_label_set_text_static(scr->status, scr->status_text);
    lv_label_set_text_static(scr->summary, scr->summary_text);
    lv_obj_set_style_text_color(scr->status,
                                any_danger ? ui_theme_amber_bright()
                                            : ui_theme_amber_dim(), 0);
    lv_obj_set_style_text_color(scr->summary, ui_theme_amber_dim(), 0);

    // Les labels à largeur intrinsèque peuvent changer de largeur avec une
    // valeur à trois chiffres. Leur repositionnement ne crée aucun objet.
    place_labels(scr);
    lv_obj_invalidate(scr->canvas);
}

void temps_screen_destroy(temps_screen_t *scr) {
    if (scr == NULL) return;
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
