#include "ui/screens/lambda_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"

#include <math.h>
#include <stdio.h>

// Repère unique du cadran ambre : 320 unités, quelle que soit la taille du
// parent. Le panneau réel est un disque de 480 px.
#define CX 160.0f
#define CY 160.0f

#define OUTER_R 151.0f
#define OUTER_W 3.0f
#define AFR_SEGMENTS 32
#define AFR_MIN 10.0f
#define AFR_MAX 20.0f
#define AFR_DANGER 17.0f

#define METER_MIN_Y 74.0f
#define METER_MAX_Y 124.0f
#define METER_SEGMENTS 9
#define METER_X_LEFT 31.0f
#define METER_X_RIGHT 289.0f
#define METER_W 10.0f

#define DUTY_R 116.0f
#define DUTY_SEGMENTS 18
#define DUTY_START 24.0f
#define DUTY_END 156.0f

#define LINE_W 0.8f
#define ICON_W 1.2f

// Une paire de labels permet de conserver le faux-gras du style amber sans
// dépendre d'une variante bold de Michroma. Les textes sont toujours statiques
// : les buffers appartiennent à lambda_screen_s et vivent aussi longtemps que
// les labels.
typedef struct {
    lv_obj_t *shadow;
    lv_obj_t *front;
    float x;
    float y;
    int spread;
} amber_text_t;

struct lambda_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;

    amber_text_t title;
    amber_text_t subtitle;
    amber_text_t signal;
    amber_text_t afr;
    amber_text_t afr_unit;
    amber_text_t lambda_value;
    amber_text_t lambda_unit;
    amber_text_t lambda_label;
    amber_text_t o2_value;
    amber_text_t o2_unit;
    amber_text_t o2_label;
    amber_text_t duty_value;
    amber_text_t duty_unit;
    amber_text_t duty_label;

    char afr_buf[12];
    char lambda_buf[12];
    char o2_buf[12];
    char duty_buf[12];
    bool connected;
    float afr_value;
    float lambda_mv;
    float o2_mv;
    float duty;
};

static void draw_afr_ring(lv_layer_t *layer, const ui_layout_t *layout,
                          const lambda_screen_t *screen) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();
    const float progress = amber_progress(screen->afr_value, AFR_MIN, AFR_MAX);
    const int lit = (int)lroundf(progress * AFR_SEGMENTS);
    const float gap = 2.0f;
    const float bar = (360.0f - AFR_SEGMENTS * gap) / AFR_SEGMENTS;

    // Couronne d'instrument : chaque cellule a des bords francs et un espace
    // constant, comme le compte-tours du style amber.
    for (int i = 0; i < AFR_SEGMENTS; i++) {
        const float start = (float)i * (bar + gap) + gap * 0.5f;
        const lv_color_t color = (screen->connected && i < lit) ? bright : dim;
        amber_draw_arc(layer, layout, CX, CY, OUTER_R, OUTER_W, start,
                       start + bar, color, false);
    }

    // Fenêtre de mélange nominale : deux repères fins, jamais une couleur
    // d'alerte. Ils rendent la zone de référence lisible sous toute intensité.
    const float nominal_low = amber_progress(14.0f, AFR_MIN, AFR_MAX) * 360.0f;
    const float nominal_high = amber_progress(15.2f, AFR_MIN, AFR_MAX) * 360.0f;
    amber_draw_arc(layer, layout, CX, CY, OUTER_R + 2.5f, 0.8f,
                   nominal_low - 2.0f, nominal_low + 2.0f, separator, false);
    amber_draw_arc(layer, layout, CX, CY, OUTER_R + 2.5f, 0.8f,
                   nominal_high - 2.0f, nominal_high + 2.0f, separator, false);

    // Le seuil AFR est une rupture de structure. Hors plage, un second trait
    // autour du curseur renforce l'information sans introduire rouge ou vert.
    const float danger_angle = amber_progress(AFR_DANGER, AFR_MIN, AFR_MAX) * 360.0f;
    amber_draw_tick(layer, layout, CX, CY, danger_angle, OUTER_R - 4.0f,
                    OUTER_R + 4.0f, ICON_W, separator, false);
    if (screen->afr_value >= AFR_DANGER) {
        amber_draw_tick(layer, layout, CX, CY, danger_angle - 3.0f,
                        OUTER_R - 7.0f, OUTER_R + 4.0f, ICON_W, bright, false);
        amber_draw_tick(layer, layout, CX, CY, danger_angle + 3.0f,
                        OUTER_R - 7.0f, OUTER_R + 4.0f, ICON_W, bright, false);
    }
}

static void draw_sensor_meter(lv_layer_t *layer, const ui_layout_t *layout,
                              float x, float value, lv_color_t color) {
    const int lit = (int)lroundf(amber_progress(value, 0.0f, 1000.0f) * METER_SEGMENTS);
    const float step = (METER_MAX_Y - METER_MIN_Y) / METER_SEGMENTS;

    for (int i = 0; i < METER_SEGMENTS; i++) {
        const float y = METER_MAX_Y - (i + 0.5f) * step;
        const lv_color_t segment = i < lit ? color : ui_theme_amber_dim();
        amber_draw_line(layer, layout, x - METER_W * 0.5f, y,
                        x + METER_W * 0.5f, y, 2.0f, segment, false);
    }

    // Petite échelle externe : elle distingue la télémétrie de la valeur AFR.
    amber_draw_line(layer, layout, x - METER_W * 0.5f - 4.0f, METER_MIN_Y,
                    x - METER_W * 0.5f - 4.0f, METER_MAX_Y,
                    LINE_W, ui_theme_amber_separator(), false);
}

static void draw_duty_meter(lv_layer_t *layer, const ui_layout_t *layout,
                            const lambda_screen_t *screen) {
    const float progress = amber_progress(screen->duty, 0.0f, 100.0f);
    const int lit = (int)lroundf(progress * DUTY_SEGMENTS);
    const float gap = 2.0f;
    const float step = (DUTY_END - DUTY_START) / DUTY_SEGMENTS;

    for (int i = 0; i < DUTY_SEGMENTS; i++) {
        const float start = DUTY_START + i * step + gap * 0.5f;
        const float end = DUTY_START + (i + 1) * step - gap * 0.5f;
        const lv_color_t color = (screen->connected && i < lit)
                                     ? ui_theme_amber_bright()
                                     : ui_theme_amber_dim();
        amber_draw_arc(layer, layout, CX, CY, DUTY_R, 2.0f, start, end,
                       color, false);
    }

    // Repère zéro central, volontairement séparé de la couronne.
    amber_draw_dot(layer, layout, CX, CY + DUTY_R, 1.4f,
                   ui_theme_amber_separator());
}

static void canvas_draw_cb(lv_event_t *event) {
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lambda_screen_t *screen = lv_obj_get_user_data(canvas);
    if (layer == NULL || screen == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    ui_layout_t layout = amber_draw_layout(&area);
    layout.ox += area.x1;
    layout.oy += area.y1;

    draw_afr_ring(layer, &layout, screen);
    draw_sensor_meter(layer, &layout, METER_X_LEFT, screen->lambda_mv,
                      screen->connected ? ui_theme_amber_bright()
                                         : ui_theme_amber_dim());
    draw_sensor_meter(layer, &layout, METER_X_RIGHT, screen->o2_mv,
                      screen->connected ? ui_theme_amber_bright()
                                         : ui_theme_amber_dim());
    draw_duty_meter(layer, &layout, screen);

    // Architecture ouverte : les filets s'arrêtent avant les blocs de texte.
    amber_draw_line(layer, &layout, 48.0f, 61.0f, 125.0f, 61.0f,
                    LINE_W, ui_theme_amber_separator(), false);
    amber_draw_line(layer, &layout, 195.0f, 61.0f, 272.0f, 61.0f,
                    LINE_W, ui_theme_amber_separator(), false);
    amber_draw_line(layer, &layout, 78.0f, 190.0f, 122.0f, 190.0f,
                    LINE_W, ui_theme_amber_separator(), false);
    amber_draw_line(layer, &layout, 198.0f, 190.0f, 242.0f, 190.0f,
                    LINE_W, ui_theme_amber_separator(), false);

    // Un noyau en deux arcs apporte une profondeur de cadran sans fermer la
    // lecture de la valeur centrale.
    amber_draw_arc(layer, &layout, CX, CY, 82.0f, LINE_W, 196.0f, 344.0f,
                   ui_theme_amber_separator(), false);
    amber_draw_arc(layer, &layout, CX, CY, 88.0f, 0.7f, 16.0f, 164.0f,
                   ui_theme_amber_dim(), false);
}

static void text_style(amber_text_t *text, lv_color_t front,
                       lv_color_t shadow) {
    lv_obj_set_style_text_color(text->front, front, 0);
    lv_obj_set_style_text_color(text->shadow, shadow, 0);
}

static void text_place(amber_text_t *text, lv_obj_t *parent) {
    const lv_area_t area = {
        0, 0, lv_obj_get_width(parent) - 1, lv_obj_get_height(parent) - 1
    };
    const ui_layout_t layout = amber_draw_layout(&area);
    const int32_t dx = lroundf(text->spread * layout.scale * UI_REFERENCE_SIZE /
                               UI_DISPLAY_SIZE_PX);

    lv_obj_update_layout(text->shadow);
    lv_obj_update_layout(text->front);
    const int32_t w_shadow = lv_obj_get_width(text->shadow);
    const int32_t h_shadow = lv_obj_get_height(text->shadow);
    const int32_t w_front = lv_obj_get_width(text->front);
    const int32_t h_front = lv_obj_get_height(text->front);

    lv_obj_set_pos(text->shadow,
                   lroundf(ui_layout_x(&layout, text->x)) - w_shadow / 2 + dx,
                   lroundf(ui_layout_y(&layout, text->y)) - h_shadow / 2);
    lv_obj_set_pos(text->front,
                   lroundf(ui_layout_x(&layout, text->x)) - w_front / 2 - dx,
                   lroundf(ui_layout_y(&layout, text->y)) - h_front / 2);
}

static amber_text_t text_create(lv_obj_t *parent, const lv_font_t *font,
                                float x, float y, int spread,
                                const char *initial, lv_color_t front,
                                lv_color_t shadow) {
    amber_text_t text = {0};
    text.x = x;
    text.y = y;
    text.spread = spread;

    text.shadow = lv_label_create(parent);
    if (text.shadow == NULL) return text;
    text.front = lv_label_create(parent);
    if (text.front == NULL) {
        lv_obj_delete(text.shadow);
        text.shadow = NULL;
        return text;
    }

    lv_obj_set_style_text_font(text.shadow, font, 0);
    lv_obj_set_style_text_align(text.shadow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_all(text.shadow, 0, 0);
    lv_label_set_text_static(text.shadow, initial);

    lv_obj_set_style_text_font(text.front, font, 0);
    lv_obj_set_style_text_align(text.front, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_all(text.front, 0, 0);
    lv_label_set_text_static(text.front, initial);
    text_style(&text, front, shadow);
    text_place(&text, parent);
    return text;
}

static bool text_valid(const amber_text_t *text) {
    return text->shadow != NULL && text->front != NULL;
}

static void text_set(amber_text_t *text, lv_obj_t *parent, const char *value) {
    if (!text_valid(text)) return;
    lv_label_set_text_static(text->shadow, value);
    lv_label_set_text_static(text->front, value);
    text_place(text, parent);
}

static void text_destroy(amber_text_t *text) {
    // Les labels sont enfants de root ; root est supprimé en une seule fois.
    text->shadow = NULL;
    text->front = NULL;
}

lambda_screen_t *lambda_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    lambda_screen_t *screen = lv_malloc(sizeof(*screen));
    if (screen == NULL) return NULL;
    lv_memzero(screen, sizeof(*screen));

    lv_obj_set_style_bg_color(parent, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_update_layout(parent);

    const int32_t side = LV_MIN(lv_obj_get_content_width(parent),
                                lv_obj_get_content_height(parent));
    if (side <= 0) goto fail;

    screen->root = lv_obj_create(parent);
    if (screen->root == NULL) goto fail;
    lv_obj_remove_style_all(screen->root);
    lv_obj_set_size(screen->root, side, side);
    lv_obj_center(screen->root);
    lv_obj_clear_flag(screen->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_update_layout(screen->root);

    screen->canvas = lv_obj_create(screen->root);
    if (screen->canvas == NULL) goto fail;
    lv_obj_remove_style_all(screen->canvas);
    lv_obj_set_size(screen->canvas, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(screen->canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(screen->canvas, screen);
    lv_obj_add_event_cb(screen->canvas, canvas_draw_cb, LV_EVENT_DRAW_MAIN,
                        NULL);

    const lv_font_t *title_font = ui_font_or(ui_font_m, &lv_font_montserrat_20);
    const lv_font_t *body_font = ui_font_or(ui_font_l, &lv_font_montserrat_20);
    const lv_font_t *caption_font = &lv_font_montserrat_14;
    const lv_font_t *hero_font = ui_font_or(ui_font_xl, &lv_font_montserrat_48);
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();

    screen->title = text_create(screen->root, title_font, CX, 30.0f, 1,
                                 "WIDEBAND / AFR", bright, separator);
    screen->subtitle = text_create(screen->root, caption_font, CX, 48.0f, 0,
                                   "SENSOR ARRAY", dim, dim);
    screen->signal = text_create(screen->root, caption_font, CX, 65.0f, 0,
                                 "SIGNAL LIVE", bright, separator);

    screen->afr = text_create(screen->root, hero_font, CX, 141.0f, 2, "0.00",
                              bright, dim);
    screen->afr_unit = text_create(screen->root, body_font, CX, 177.0f, 1,
                                   "AIR / FUEL", bright, separator);

    screen->lambda_value = text_create(screen->root, body_font, 67.0f, 95.0f,
                                       1, "0", bright, separator);
    screen->lambda_unit = text_create(screen->root, caption_font, 67.0f, 111.0f,
                                      0, "mV", dim, dim);
    screen->lambda_label = text_create(screen->root, caption_font, 67.0f, 130.0f,
                                       0, "LAMBDA", dim, dim);

    screen->o2_value = text_create(screen->root, body_font, 253.0f, 95.0f,
                                   1, "0", bright, separator);
    screen->o2_unit = text_create(screen->root, caption_font, 253.0f, 111.0f,
                                  0, "mV", dim, dim);
    screen->o2_label = text_create(screen->root, caption_font, 253.0f, 130.0f,
                                   0, "O2 SENSOR", dim, dim);

    screen->duty_value = text_create(screen->root, body_font, CX, 230.0f, 1,
                                     "0", bright, separator);
    screen->duty_unit = text_create(screen->root, caption_font, CX, 247.0f, 0,
                                    "%", dim, dim);
    screen->duty_label = text_create(screen->root, caption_font, CX, 266.0f, 0,
                                     "HEATER DUTY", dim, dim);

    if (!text_valid(&screen->title) || !text_valid(&screen->subtitle) ||
        !text_valid(&screen->signal) || !text_valid(&screen->afr) ||
        !text_valid(&screen->afr_unit) || !text_valid(&screen->lambda_value) ||
        !text_valid(&screen->lambda_unit) ||
        !text_valid(&screen->lambda_label) || !text_valid(&screen->o2_value) ||
        !text_valid(&screen->o2_unit) || !text_valid(&screen->o2_label) ||
        !text_valid(&screen->duty_value) || !text_valid(&screen->duty_unit) ||
        !text_valid(&screen->duty_label)) {
        goto fail;
    }

    return screen;

fail:
    lambda_screen_destroy(screen);
    return NULL;
}

void lambda_screen_update(lambda_screen_t *screen, const ecu_data_t *data) {
    if (screen == NULL || data == NULL || screen->canvas == NULL) return;

    screen->connected = data->connected;
    screen->afr_value = isfinite(data->estimated_air_fuel)
                            ? data->estimated_air_fuel
                            : 0.0f;
    screen->lambda_mv = isfinite(data->lambda_mv)
                            ? amber_clampf(data->lambda_mv, 0.0f, 1000.0f)
                            : 0.0f;
    screen->o2_mv = isfinite(data->o2_mv)
                        ? amber_clampf(data->o2_mv, 0.0f, 1000.0f)
                        : 0.0f;
    screen->duty = isfinite(data->lambda_sensor_duty_cycle)
                       ? amber_clampf(data->lambda_sensor_duty_cycle, 0.0f, 100.0f)
                       : 0.0f;

    snprintf(screen->afr_buf, sizeof(screen->afr_buf), "%.2f",
             screen->afr_value);
    snprintf(screen->lambda_buf, sizeof(screen->lambda_buf), "%.0f",
             screen->lambda_mv);
    snprintf(screen->o2_buf, sizeof(screen->o2_buf), "%.0f", screen->o2_mv);
    snprintf(screen->duty_buf, sizeof(screen->duty_buf), "%.0f", screen->duty);

    text_set(&screen->afr, screen->root, screen->afr_buf);
    text_set(&screen->lambda_value, screen->root, screen->lambda_buf);
    text_set(&screen->o2_value, screen->root, screen->o2_buf);
    text_set(&screen->duty_value, screen->root, screen->duty_buf);
    text_set(&screen->signal, screen->root,
             screen->connected ? "SIGNAL LIVE" : "SIGNAL LOST");

    text_style(&screen->signal,
               screen->connected ? ui_theme_amber_bright()
                                  : ui_theme_amber_dim(),
               ui_theme_amber_separator());
    lv_obj_invalidate(screen->canvas);
}

void lambda_screen_destroy(lambda_screen_t *screen) {
    if (screen == NULL) return;

    text_destroy(&screen->title);
    text_destroy(&screen->subtitle);
    text_destroy(&screen->signal);
    text_destroy(&screen->afr);
    text_destroy(&screen->afr_unit);
    text_destroy(&screen->lambda_value);
    text_destroy(&screen->lambda_unit);
    text_destroy(&screen->lambda_label);
    text_destroy(&screen->o2_value);
    text_destroy(&screen->o2_unit);
    text_destroy(&screen->o2_label);
    text_destroy(&screen->duty_value);
    text_destroy(&screen->duty_unit);
    text_destroy(&screen->duty_label);

    if (screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
