#include "ui/screens/injection_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"

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
#define ICON_LINE_WIDTH       1.8f

#define HEADER_Y             25.0f
#define SUBHEADER_Y          44.0f
#define HERO_Y              101.0f
#define HERO_LABEL_Y        139.0f
#define PANEL_TOP_Y         165.0f
#define TRIM_LABEL_Y        177.0f
#define TRIM_VALUE_Y        195.0f
#define TRIM_BAR_Y          216.0f
#define INJ_DIVIDER_Y       232.0f
#define INJ_ICON_Y          246.0f
#define INJ_VALUE_Y         264.0f
#define INJ_LABEL_Y         283.0f

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

static ui_layout_t layout_of(const lv_area_t *area) {
    ui_layout_t layout = ui_layout_fit(lv_area_get_width(area),
                                        lv_area_get_height(area));
    layout.ox += area->x1;
    layout.oy += area->y1;
    return layout;
}

static float clampf(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

static void draw_line(lv_layer_t *layer, const ui_layout_t *layout,
                      float x1, float y1, float x2, float y2, float width,
                      lv_color_t color, bool rounded) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.width = LV_MAX(1, (int32_t)lroundf(width * layout->scale));
    dsc.round_start = rounded;
    dsc.round_end = rounded;
    dsc.p1.x = lroundf(ui_layout_x(layout, x1));
    dsc.p1.y = lroundf(ui_layout_y(layout, y1));
    dsc.p2.x = lroundf(ui_layout_x(layout, x2));
    dsc.p2.y = lroundf(ui_layout_y(layout, y2));
    lv_draw_line(layer, &dsc);
}

static void draw_arc(lv_layer_t *layer, const ui_layout_t *layout,
                     float radius, float width, float start, float end,
                     lv_color_t color) {
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.center.x = lroundf(ui_layout_x(layout, SCREEN_CX));
    dsc.center.y = lroundf(ui_layout_y(layout, SCREEN_CY));
    dsc.radius = LV_MAX(1, (int32_t)lroundf(radius * layout->scale));
    dsc.width = LV_MAX(1, (int32_t)lroundf(width * layout->scale));
    dsc.start_angle = start;
    dsc.end_angle = end;
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.rounded = 0;
    lv_draw_arc(layer, &dsc);
}

static void draw_ring(lv_layer_t *layer, const ui_layout_t *layout,
                      float feedback, bool connected) {
    const float total_gap = (RING_SEGMENTS - 1) * RING_GAP_DEG;
    const float segment_deg = (360.0f - total_gap) / RING_SEGMENTS;
    const float pitch_deg = segment_deg + RING_GAP_DEG;
    const float progress = connected
        ? clampf(feedback / FEEDBACK_MAX, 0.0f, 1.0f)
        : 0.0f;
    const int lit = (int)lroundf(progress * RING_SEGMENTS);

    // Une couronne instrumentée complète : elle donne la présence d'un
    // cadran automobile tout en conservant une lecture immédiate du feedback.
    for (int i = 0; i < RING_SEGMENTS; i++) {
        const float start = -90.0f + i * pitch_deg;
        draw_arc(layer, layout, OUTER_RING_R, OUTER_RING_WIDTH, start,
                 start + segment_deg,
                 i < lit ? ui_theme_amber_bright() : ui_theme_amber_dim());
    }

    // Guides ouverts : ils structurent la zone centrale sans enfermer les
    // informations dans une grille lourde.
    draw_arc(layer, layout, GUIDE_R, GUIDE_WIDTH, 208.0f, 332.0f,
             ui_theme_amber_separator());
}

static void draw_trim_scale(lv_layer_t *layer, const ui_layout_t *layout,
                            float center_x, float value) {
    const float left = center_x - 34.0f;
    const float right = center_x + 34.0f;
    const float marker = center_x + clampf(value, -30.0f, 30.0f) / 30.0f * 30.0f;

    draw_line(layer, layout, left, TRIM_BAR_Y, right, TRIM_BAR_Y,
              PANEL_LINE_WIDTH, ui_theme_amber_separator(), false);
    draw_line(layer, layout, center_x, TRIM_BAR_Y - 4.0f,
              center_x, TRIM_BAR_Y + 4.0f, PANEL_LINE_WIDTH,
              ui_theme_amber_separator(), false);
    draw_line(layer, layout, left, TRIM_BAR_Y - 2.0f,
              left, TRIM_BAR_Y + 2.0f, PANEL_LINE_WIDTH,
              ui_theme_amber_separator(), false);
    draw_line(layer, layout, right, TRIM_BAR_Y - 2.0f,
              right, TRIM_BAR_Y + 2.0f, PANEL_LINE_WIDTH,
              ui_theme_amber_separator(), false);
    draw_line(layer, layout, marker, TRIM_BAR_Y - 6.0f,
              marker, TRIM_BAR_Y + 6.0f, 2.0f,
              ui_theme_amber_bright(), false);
}

static void draw_injector_icon(lv_layer_t *layer, const ui_layout_t *layout,
                               float center_x) {
    const lv_color_t color = ui_theme_amber_bright();

    // Injecteur stylisé : corps, prise électrique, rampe et pointe. Toutes
    // les formes sont des traits LVGL, donc aucune image n'est embarquée.
    draw_line(layer, layout, center_x - 10.0f, INJ_ICON_Y - 9.0f,
              center_x + 7.0f, INJ_ICON_Y - 9.0f, ICON_LINE_WIDTH,
              color, false);
    draw_line(layer, layout, center_x + 7.0f, INJ_ICON_Y - 9.0f,
              center_x + 7.0f, INJ_ICON_Y + 2.0f, ICON_LINE_WIDTH,
              color, false);
    draw_line(layer, layout, center_x + 7.0f, INJ_ICON_Y + 2.0f,
              center_x + 2.0f, INJ_ICON_Y + 8.0f, ICON_LINE_WIDTH,
              color, false);
    draw_line(layer, layout, center_x + 2.0f, INJ_ICON_Y + 8.0f,
              center_x + 2.0f, INJ_ICON_Y + 14.0f, ICON_LINE_WIDTH,
              color, false);
    draw_line(layer, layout, center_x - 10.0f, INJ_ICON_Y - 9.0f,
              center_x - 10.0f, INJ_ICON_Y - 3.0f, ICON_LINE_WIDTH,
              color, false);
    draw_line(layer, layout, center_x - 15.0f, INJ_ICON_Y - 3.0f,
              center_x - 10.0f, INJ_ICON_Y - 3.0f, ICON_LINE_WIDTH,
              color, false);
    draw_line(layer, layout, center_x - 15.0f, INJ_ICON_Y - 3.0f,
              center_x - 15.0f, INJ_ICON_Y + 4.0f, ICON_LINE_WIDTH,
              color, false);
    draw_line(layer, layout, center_x - 15.0f, INJ_ICON_Y + 4.0f,
              center_x - 4.0f, INJ_ICON_Y + 4.0f, ICON_LINE_WIDTH,
              color, false);
    draw_line(layer, layout, center_x - 4.0f, INJ_ICON_Y + 4.0f,
              center_x - 1.0f, INJ_ICON_Y + 9.0f, ICON_LINE_WIDTH,
              color, false);
}

static void canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *canvas = lv_event_get_target(event);
    injection_screen_t *scr = lv_obj_get_user_data(canvas);
    lv_layer_t *layer = lv_event_get_layer(event);
    if (scr == NULL || layer == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = layout_of(&area);

    draw_ring(layer, &layout, scr->feedback, scr->connected);
    draw_trim_scale(layer, &layout, 82.0f, scr->short_trim);
    draw_trim_scale(layer, &layout, 238.0f, scr->long_trim);
    draw_injector_icon(layer, &layout, 82.0f);
    draw_injector_icon(layer, &layout, 238.0f);

    // Séparateurs interrompus à chaque cellule : le cadran reste ouvert et
    // les extrémités suivent visuellement la courbure du disque.
    draw_line(layer, &layout, 42.0f, PANEL_TOP_Y, 132.0f,
              PANEL_TOP_Y, PANEL_LINE_WIDTH, ui_theme_amber_separator(), false);
    draw_line(layer, &layout, 188.0f, PANEL_TOP_Y, 278.0f,
              PANEL_TOP_Y, PANEL_LINE_WIDTH, ui_theme_amber_separator(), false);
    draw_line(layer, &layout, 160.0f, PANEL_TOP_Y + 7.0f, 160.0f,
              INJ_DIVIDER_Y - 7.0f, PANEL_LINE_WIDTH,
              ui_theme_amber_separator(), false);
    draw_line(layer, &layout, 42.0f, INJ_DIVIDER_Y, 132.0f,
              INJ_DIVIDER_Y, PANEL_LINE_WIDTH, ui_theme_amber_separator(), false);
    draw_line(layer, &layout, 188.0f, INJ_DIVIDER_Y, 278.0f,
              INJ_DIVIDER_Y, PANEL_LINE_WIDTH, ui_theme_amber_separator(), false);
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font,
                            lv_color_t color, const char *text,
                            int32_t width) {
    lv_obj_t *label = lv_label_create(parent);
    if (label == NULL) return NULL;

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(label, width);
    lv_label_set_text_static(label, text != NULL ? text : "");
    return label;
}

static void place_label(lv_obj_t *label, lv_obj_t *parent, float x, float y,
                        int32_t width, int spread) {
    const ui_layout_t layout = ui_layout_fit(lv_obj_get_width(parent),
                                              lv_obj_get_height(parent));
    lv_obj_set_width(label, lroundf(width * layout.scale));
    lv_obj_update_layout(label);
    lv_obj_set_pos(label,
                   lroundf(ui_layout_x(&layout, x)) - lv_obj_get_width(label) / 2
                       + lroundf(spread * layout.scale * UI_REFERENCE_SIZE /
                                 UI_DISPLAY_SIZE_PX),
                   lroundf(ui_layout_y(&layout, y)) - lv_obj_get_height(label) / 2);
}

static void place_pair(lv_obj_t *front, lv_obj_t *shadow, lv_obj_t *parent,
                       float x, float y, int32_t width, int spread) {
    place_label(shadow, parent, x, y, width, spread);
    place_label(front, parent, x, y, width, -spread);
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
             sizeof(scr->metric_text[METRIC_INJECTOR_1]), "%.2fms",
             data->injector_1_pw);
    snprintf(scr->metric_text[METRIC_INJECTOR_2],
             sizeof(scr->metric_text[METRIC_INJECTOR_2]), "%.2fms",
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

    const int32_t side = LV_MIN(lv_obj_get_content_width(parent),
                                lv_obj_get_content_height(parent));
    if (side <= 0) goto fail;

    scr->root = lv_obj_create(parent);
    if (scr->root == NULL) goto fail;
    lv_obj_remove_style_all(scr->root);
    lv_obj_set_size(scr->root, side, side);
    lv_obj_center(scr->root);
    lv_obj_set_style_bg_color(scr->root, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(scr->root, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(scr->root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(scr->root, true, 0);
    lv_obj_clear_flag(scr->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_update_layout(scr->root);

    scr->canvas = lv_obj_create(scr->root);
    if (scr->canvas == NULL) goto fail;
    lv_obj_remove_style_all(scr->canvas);
    lv_obj_set_size(scr->canvas, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(scr->canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(scr->canvas, scr);
    lv_obj_add_event_cb(scr->canvas, canvas_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    const lv_font_t *font_xl = ui_font_or(ui_font_xl, &lv_font_montserrat_48);
    const lv_font_t *font_l = ui_font_or(ui_font_l, &lv_font_montserrat_20);
    const lv_font_t *font_m = ui_font_or(ui_font_m, &lv_font_montserrat_14);
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    lv_obj_t *header = make_label(scr->root, font_l, bright, "INJECTION", 180);
    lv_obj_t *subheader = make_label(scr->root, font_m, dim,
                                     "FUEL CONTROL / ECU", 220);
    lv_obj_t *hero_label = make_label(scr->root, font_m, bright,
                                      "FEEDBACK", 150);
    lv_obj_t *trim_short_label = make_label(scr->root, font_m, dim,
                                            "ST TRIM", 110);
    lv_obj_t *trim_long_label = make_label(scr->root, font_m, dim,
                                           "LT TRIM", 110);
    lv_obj_t *inj_one_label = make_label(scr->root, font_m, dim,
                                         "INJ 1", 110);
    lv_obj_t *inj_two_label = make_label(scr->root, font_m, dim,
                                         "INJ 2", 110);

    set_unavailable_text(scr);
    scr->hero_front = make_label(scr->root, font_xl, bright, scr->hero_text, 190);
    scr->hero_shadow = make_label(scr->root, font_xl, bright, scr->hero_text, 190);
    for (int i = 0; i < METRIC_COUNT; i++) {
        scr->metric_front[i] = make_label(scr->root, font_l, bright,
                                          scr->metric_text[i], 116);
        scr->metric_shadow[i] = make_label(scr->root, font_l, bright,
                                           scr->metric_text[i], 116);
    }

    if (header == NULL || subheader == NULL || hero_label == NULL ||
        trim_short_label == NULL || trim_long_label == NULL ||
        inj_one_label == NULL || inj_two_label == NULL ||
        scr->hero_front == NULL || scr->hero_shadow == NULL) goto fail;
    for (int i = 0; i < METRIC_COUNT; i++) {
        if (scr->metric_front[i] == NULL || scr->metric_shadow[i] == NULL) goto fail;
    }

    place_label(header, scr->root, SCREEN_CX, HEADER_Y, 180, 0);
    place_label(subheader, scr->root, SCREEN_CX, SUBHEADER_Y, 220, 0);
    place_pair(scr->hero_front, scr->hero_shadow, scr->root,
               SCREEN_CX, HERO_Y, 190, 2);
    place_label(hero_label, scr->root, SCREEN_CX, HERO_LABEL_Y, 150, 0);

    place_label(trim_short_label, scr->root, 82.0f, TRIM_LABEL_Y, 110, 0);
    place_label(trim_long_label, scr->root, 238.0f, TRIM_LABEL_Y, 110, 0);
    place_pair(scr->metric_front[METRIC_SHORT_TRIM],
               scr->metric_shadow[METRIC_SHORT_TRIM], scr->root,
               82.0f, TRIM_VALUE_Y, 116, 1);
    place_pair(scr->metric_front[METRIC_LONG_TRIM],
               scr->metric_shadow[METRIC_LONG_TRIM], scr->root,
               238.0f, TRIM_VALUE_Y, 116, 1);

    place_label(inj_one_label, scr->root, 82.0f, INJ_LABEL_Y, 110, 0);
    place_label(inj_two_label, scr->root, 238.0f, INJ_LABEL_Y, 110, 0);
    place_pair(scr->metric_front[METRIC_INJECTOR_1],
               scr->metric_shadow[METRIC_INJECTOR_1], scr->root,
               82.0f, INJ_VALUE_Y, 116, 1);
    place_pair(scr->metric_front[METRIC_INJECTOR_2],
               scr->metric_shadow[METRIC_INJECTOR_2], scr->root,
               238.0f, INJ_VALUE_Y, 116, 1);

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
