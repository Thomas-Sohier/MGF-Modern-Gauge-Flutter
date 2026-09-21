#include "ui/screens/injection_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>

// Repère de conception commun aux écrans ambre : 320 unités sur le diamètre.
#define SCREEN_CX 160.0f
#define SCREEN_CY 160.0f

#define OUTER_RING_R       151.0f
#define OUTER_RING_WIDTH     3.0f
#define RING_SEGMENTS       32
#define RING_GAP_DEG         2.2f
#define FEEDBACK_MAX       200.0f

#define GUIDE_R             126.0f
#define GUIDE_WIDTH           0.8f
#define PANEL_LINE_WIDTH      0.8f
#define HEADER_Y             27.0f
#define SUBHEADER_Y          49.0f
#define HERO_Y               91.0f
#define HERO_LABEL_Y        124.0f
#define PANEL_TOP_Y         151.0f
#define TRIM_LABEL_Y        169.0f
#define TRIM_VALUE_Y        196.0f
#define TRIM_BAR_Y          211.0f
#define INJ_DIVIDER_Y       223.0f
#define INJ_VALUE_Y         262.0f
#define INJ_LABEL_Y         241.0f

#define METRIC_COUNT 4
enum {
    METRIC_SHORT_TRIM = 0,
    METRIC_LONG_TRIM,
    METRIC_INJECTOR_1,
    METRIC_INJECTOR_2
};

struct injection_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;
    lv_obj_t *hero_front;
    lv_obj_t *hero_shadow;
    lv_obj_t *metric_front[METRIC_COUNT];
    lv_obj_t *metric_shadow[METRIC_COUNT];
    bool connected;
    float feedback;
    float short_trim;
    float long_trim;
    char hero_text[16];
    char metric_text[METRIC_COUNT][20];
};

static void draw_ring(lv_layer_t *layer, const ui_layout_t *layout,
                      float feedback, bool connected) {
    const float total_gap = (RING_SEGMENTS - 1) * RING_GAP_DEG;
    const float segment_deg = (360.0f - total_gap) / RING_SEGMENTS;
    const float pitch_deg = segment_deg + RING_GAP_DEG;
    const float progress = connected
        ? amber_clampf(feedback / FEEDBACK_MAX, 0.0f, 1.0f)
        : 0.0f;
    const int lit = (int)lroundf(progress * RING_SEGMENTS);

    // Une couronne instrumentée complète : elle donne la présence d'un
    // cadran automobile tout en conservant une lecture immédiate du feedback.
    for (int i = 0; i < RING_SEGMENTS; i++) {
        const float start = -90.0f + i * pitch_deg;
        amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY,
                       OUTER_RING_R, OUTER_RING_WIDTH, start,
                       start + segment_deg,
                       i < lit ? ui_theme_amber_bright() : ui_theme_amber_dim(),
                       false);
    }

    // Guides ouverts : ils structurent la zone centrale sans enfermer les
    // informations dans une grille lourde.
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, GUIDE_R, GUIDE_WIDTH,
                   208.0f, 332.0f, ui_theme_amber_separator(), false);
}

static void draw_trim_scale(lv_layer_t *layer, const ui_layout_t *layout,
                            float center_x, float value) {
    const float left = center_x - 34.0f;
    const float right = center_x + 34.0f;
    const float marker = center_x + amber_clampf(value, -30.0f, 30.0f) / 30.0f * 30.0f;

    amber_draw_line(layer, layout, left, TRIM_BAR_Y, right, TRIM_BAR_Y,
                    PANEL_LINE_WIDTH, ui_theme_amber_separator(), false);
    amber_draw_line(layer, layout, center_x, TRIM_BAR_Y - 4.0f,
                    center_x, TRIM_BAR_Y + 4.0f, PANEL_LINE_WIDTH,
                    ui_theme_amber_separator(), false);
    amber_draw_line(layer, layout, left, TRIM_BAR_Y - 2.0f,
                    left, TRIM_BAR_Y + 2.0f, PANEL_LINE_WIDTH,
                    ui_theme_amber_separator(), false);
    amber_draw_line(layer, layout, right, TRIM_BAR_Y - 2.0f,
                    right, TRIM_BAR_Y + 2.0f, PANEL_LINE_WIDTH,
                    ui_theme_amber_separator(), false);
    amber_draw_line(layer, layout, marker, TRIM_BAR_Y - 6.0f,
                    marker, TRIM_BAR_Y + 6.0f, 2.0f,
                    ui_theme_amber_bright(), false);
}

static void canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *canvas = lv_event_get_target(event);
    injection_screen_t *scr = lv_obj_get_user_data(canvas);
    lv_layer_t *layer = lv_event_get_layer(event);
    if (scr == NULL || layer == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    ui_layout_t layout = amber_draw_layout(&area);
    layout.ox += area.x1;
    layout.oy += area.y1;

    draw_ring(layer, &layout, scr->feedback, scr->connected);
    draw_trim_scale(layer, &layout, 96.0f, scr->short_trim);
    draw_trim_scale(layer, &layout, 224.0f, scr->long_trim);

    // Grille ouverte : chaque séparateur s'arrête avant la cellule voisine.
    // Les deux colonnes restent ainsi lisibles jusque dans la courbure basse.
    amber_draw_line(layer, &layout, 48.0f, PANEL_TOP_Y, 143.0f,
                    PANEL_TOP_Y, PANEL_LINE_WIDTH,
                    ui_theme_amber_separator(), false);
    amber_draw_line(layer, &layout, 177.0f, PANEL_TOP_Y, 272.0f,
                    PANEL_TOP_Y, PANEL_LINE_WIDTH,
                    ui_theme_amber_separator(), false);
    amber_draw_line(layer, &layout, 160.0f, PANEL_TOP_Y + 7.0f, 160.0f,
                    INJ_DIVIDER_Y - 7.0f, PANEL_LINE_WIDTH,
                    ui_theme_amber_separator(), false);
    amber_draw_line(layer, &layout, 48.0f, INJ_DIVIDER_Y, 143.0f,
                    INJ_DIVIDER_Y, PANEL_LINE_WIDTH,
                    ui_theme_amber_separator(), false);
    amber_draw_line(layer, &layout, 177.0f, INJ_DIVIDER_Y, 272.0f,
                    INJ_DIVIDER_Y, PANEL_LINE_WIDTH,
                    ui_theme_amber_separator(), false);
    amber_draw_line(layer, &layout, 160.0f, INJ_DIVIDER_Y + 7.0f, 160.0f,
                    INJ_VALUE_Y + 9.0f, PANEL_LINE_WIDTH,
                    ui_theme_amber_separator(), false);
}

static void set_pair_text(injection_screen_t *scr, int index, const char *text) {
    lv_label_set_text_static(scr->metric_front[index], text);
    lv_label_set_text_static(scr->metric_shadow[index], text);
}

static void set_available_text(injection_screen_t *scr, const ecu_data_t *data) {
    snprintf(scr->hero_text, sizeof(scr->hero_text), "%.0f%%", data->fuelling_feedback_percent);
    snprintf(scr->metric_text[METRIC_SHORT_TRIM],
             sizeof(scr->metric_text[METRIC_SHORT_TRIM]), "%+.1f%%",
             data->short_term_trim_percent);
    snprintf(scr->metric_text[METRIC_LONG_TRIM],
             sizeof(scr->metric_text[METRIC_LONG_TRIM]), "%+.1f%%",
             data->long_term_trim);
    snprintf(scr->metric_text[METRIC_INJECTOR_1],
             sizeof(scr->metric_text[METRIC_INJECTOR_1]), "%.2f ms",
             data->injector_1_pw);
    snprintf(scr->metric_text[METRIC_INJECTOR_2],
             sizeof(scr->metric_text[METRIC_INJECTOR_2]), "%.2f ms",
             data->injector_2_pw);
}

static void set_unavailable_text(injection_screen_t *scr) {
    snprintf(scr->hero_text, sizeof(scr->hero_text), "--");
    for (int i = 0; i < METRIC_COUNT; i++) {
        snprintf(scr->metric_text[i], sizeof(scr->metric_text[i]), "--");
    }
}

injection_screen_t *injection_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    injection_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    lv_obj_set_style_bg_color(parent, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_update_layout(parent);

    scr->root = amber_ui_root_create(parent);
    if (scr->root == NULL) goto fail;
    scr->canvas = amber_ui_canvas_create(scr->root, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    const lv_font_t *font_xl = ui_font_or(ui_font_xl, &lv_font_montserrat_48);
    const lv_font_t *font_l = ui_font_or(ui_font_l, &lv_font_montserrat_20);
    const lv_font_t *font_m = ui_font_or(ui_font_m, &lv_font_montserrat_14);
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    lv_obj_t *header = amber_ui_label_create(scr->root, font_l, bright,
                                              "INJECTION", 180.0f);
    lv_obj_t *subheader = amber_ui_label_create(scr->root, font_m, dim,
                                                "GESTION\nCARBURANT", 220.0f);
    lv_obj_t *hero_label = amber_ui_label_create(scr->root, font_m, bright,
                                                 "CORRECTION", 150.0f);
    lv_obj_t *trim_short_label = amber_ui_label_create(
        scr->root, font_m, dim, "COURT\nTERME", 130.0f);
    lv_obj_t *trim_long_label = amber_ui_label_create(
        scr->root, font_m, dim, "LONG\nTERME", 130.0f);
    lv_obj_t *inj_one_label = amber_ui_label_create(
        scr->root, font_m, dim, "INJECT. 1", 130.0f);
    lv_obj_t *inj_two_label = amber_ui_label_create(
        scr->root, font_m, dim, "INJECT. 2", 130.0f);

    set_unavailable_text(scr);
    scr->hero_front = amber_ui_label_create(scr->root, font_xl, bright,
                                             scr->hero_text, 190.0f);
    scr->hero_shadow = amber_ui_label_create(scr->root, font_xl, bright,
                                              scr->hero_text, 190.0f);
    for (int i = 0; i < METRIC_COUNT; i++) {
        scr->metric_front[i] = amber_ui_label_create(
            scr->root, font_l, bright, scr->metric_text[i], 108.0f);
        scr->metric_shadow[i] = amber_ui_label_create(
            scr->root, font_l, bright, scr->metric_text[i], 108.0f);
    }

    if (header == NULL || subheader == NULL || hero_label == NULL ||
        trim_short_label == NULL || trim_long_label == NULL ||
        inj_one_label == NULL || inj_two_label == NULL ||
        scr->hero_front == NULL || scr->hero_shadow == NULL) goto fail;
    for (int i = 0; i < METRIC_COUNT; i++) {
        if (scr->metric_front[i] == NULL || scr->metric_shadow[i] == NULL) goto fail;
    }

    amber_ui_place_centered(header, scr->root, SCREEN_CX, HEADER_Y,
                            180.0f, 0.0f);
    amber_ui_place_centered(subheader, scr->root, SCREEN_CX, SUBHEADER_Y,
                            220.0f, 0.0f);
    amber_ui_place_centered(scr->hero_shadow, scr->root, SCREEN_CX, HERO_Y,
                            190.0f, 2.0f * UI_REFERENCE_SIZE /
                                     UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(scr->hero_front, scr->root, SCREEN_CX, HERO_Y,
                            190.0f, -2.0f * UI_REFERENCE_SIZE /
                                     UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(hero_label, scr->root, SCREEN_CX, HERO_LABEL_Y,
                            150.0f, 0.0f);

    amber_ui_place_centered(trim_short_label, scr->root, 96.0f, TRIM_LABEL_Y,
                            130.0f, 0.0f);
    amber_ui_place_centered(trim_long_label, scr->root, 224.0f, TRIM_LABEL_Y,
                            130.0f, 0.0f);
    amber_ui_place_centered(scr->metric_shadow[METRIC_SHORT_TRIM], scr->root,
                            96.0f, TRIM_VALUE_Y, 108.0f,
                            UI_REFERENCE_SIZE / UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(scr->metric_front[METRIC_SHORT_TRIM], scr->root,
                            96.0f, TRIM_VALUE_Y, 108.0f,
                            -UI_REFERENCE_SIZE / UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(scr->metric_shadow[METRIC_LONG_TRIM], scr->root,
                            224.0f, TRIM_VALUE_Y, 108.0f,
                            UI_REFERENCE_SIZE / UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(scr->metric_front[METRIC_LONG_TRIM], scr->root,
                            224.0f, TRIM_VALUE_Y, 108.0f,
                            -UI_REFERENCE_SIZE / UI_DISPLAY_SIZE_PX);

    amber_ui_place_centered(inj_one_label, scr->root, 96.0f, INJ_LABEL_Y,
                            130.0f, 0.0f);
    amber_ui_place_centered(inj_two_label, scr->root, 224.0f, INJ_LABEL_Y,
                            130.0f, 0.0f);
    amber_ui_place_centered(scr->metric_shadow[METRIC_INJECTOR_1], scr->root,
                            96.0f, INJ_VALUE_Y, 108.0f,
                            UI_REFERENCE_SIZE / UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(scr->metric_front[METRIC_INJECTOR_1], scr->root,
                            96.0f, INJ_VALUE_Y, 108.0f,
                            -UI_REFERENCE_SIZE / UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(scr->metric_shadow[METRIC_INJECTOR_2], scr->root,
                            224.0f, INJ_VALUE_Y, 108.0f,
                            UI_REFERENCE_SIZE / UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(scr->metric_front[METRIC_INJECTOR_2], scr->root,
                            224.0f, INJ_VALUE_Y, 108.0f,
                            -UI_REFERENCE_SIZE / UI_DISPLAY_SIZE_PX);

    return scr;

fail:
    injection_screen_destroy(scr);
    return NULL;
}

void injection_screen_update(injection_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL || scr->canvas == NULL) return;

    scr->connected = data->connected;
    scr->feedback = data->fuelling_feedback_percent;
    scr->short_trim = data->short_term_trim_percent;
    scr->long_trim = data->long_term_trim;

    if (scr->connected) set_available_text(scr, data);
    else set_unavailable_text(scr);

    lv_label_set_text_static(scr->hero_front, scr->hero_text);
    lv_label_set_text_static(scr->hero_shadow, scr->hero_text);
    for (int i = 0; i < METRIC_COUNT; i++) {
        set_pair_text(scr, i, scr->metric_text[i]);
    }
    // Les buffers sont persistants dans scr : aucun label ne duplique de
    // chaîne pendant update, et aucune allocation n'est réalisée ici.
    lv_obj_invalidate(scr->canvas);
}

void injection_screen_destroy(injection_screen_t *scr) {
    if (scr == NULL) return;
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
