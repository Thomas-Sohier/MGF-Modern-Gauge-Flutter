#include "ui/screens/idle_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"

#include <math.h>
#include <stdio.h>

// Toutes les coordonnées sont dans le cadran de conception 320 x 320. La
// translation et l'échelle sont calculées à partir de la zone réellement
// disponible, sans hypothèse sur le parent.
#define SCREEN_CX 160.0f
#define SCREEN_CY 160.0f

#define OUTER_R 151.0f
#define RPM_RING_R 141.0f
#define RPM_RING_W 7.0f
#define RPM_MAX 2000.0f
#define RPM_RING_START 135.0f
#define RPM_RING_SWEEP 270.0f
#define RPM_SEGMENTS 28
#define RPM_SEGMENT_GAP 1.8f

// Rails latéraux très courts : ils donnent le contexte de régulation sans
// concurrencer la valeur RPM ni les quatre cartes de télémétrie.
#define SIDE_METER_X_LEFT 45.0f
#define SIDE_METER_X_RIGHT 275.0f
#define SIDE_METER_Y0 105.0f
#define SIDE_METER_Y1 147.0f
#define SIDE_METER_SEGMENTS 6
#define SIDE_METER_W 3.0f

// La grille est volontairement en deux rangées. Chaque libellé dispose ainsi
// d'une largeur réelle, et aucun texte ne vient toucher la couronne ronde.
#define TITLE_Y 36.0f
#define SUBTITLE_Y 61.0f
#define HERO_Y 119.0f
#define HERO_UNIT_Y 148.0f
#define ERROR_Y 169.0f
#define GRID_Y 187.0f
#define METRIC_VALUE_TOP_Y 203.0f
#define METRIC_NAME_TOP_Y 221.0f
#define METRIC_VALUE_BOTTOM_Y 243.0f
#define METRIC_NAME_BOTTOM_Y 261.0f
#define STATUS_Y 289.0f

#define LINE_W 0.8f
#define TICK_W 1.1f
#define BOLD_SPREAD_PX 2

#define METRIC_COUNT 4
enum {
    METRIC_SETPOINT = 0,
    METRIC_VALVE,
    METRIC_BASE,
    METRIC_ADJUSTER,
};

static const float kMetricX[METRIC_COUNT] = {110.0f, 210.0f, 110.0f, 210.0f};
static const float kMetricValueY[METRIC_COUNT] = {
    METRIC_VALUE_TOP_Y, METRIC_VALUE_TOP_Y,
    METRIC_VALUE_BOTTOM_Y, METRIC_VALUE_BOTTOM_Y,
};
static const float kMetricNameY[METRIC_COUNT] = {
    METRIC_NAME_TOP_Y, METRIC_NAME_TOP_Y,
    METRIC_NAME_BOTTOM_Y, METRIC_NAME_BOTTOM_Y,
};

// Les buffers vivent dans l'écran et sont toujours passés à
// lv_label_set_text_static(). Une mise à jour ECU ne crée donc ni label, ni
// chaîne, ni objet LVGL.
struct idle_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;

    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *hero_shadow;
    lv_obj_t *hero;
    lv_obj_t *hero_unit;
    lv_obj_t *error;
    lv_obj_t *status;
    lv_obj_t *metric_value[METRIC_COUNT];
    lv_obj_t *metric_name[METRIC_COUNT];

    char hero_text[16];
    char error_text[24];
    char status_text[24];
    char metric_text[METRIC_COUNT][16];

    float rpm;
    float setpoint;
    float valve;
    float base;
    float error_value;
    float adjuster;
    bool connected;
};

static int32_t px(float value) {
    return (int32_t)lroundf(value);
}

static void draw_segment_meter(lv_layer_t *layer, const ui_layout_t *layout,
                               float x, float value, float low, float high,
                               bool connected) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const float step = (SIDE_METER_Y1 - SIDE_METER_Y0) /
                       (float)SIDE_METER_SEGMENTS;
    const int lit = (int)lroundf(amber_progress(value, low, high) *
                                 (float)SIDE_METER_SEGMENTS);

    for (int i = 0; i < SIDE_METER_SEGMENTS; i++) {
        const float y = SIDE_METER_Y1 - ((float)i + 0.5f) * step;
        const lv_color_t color = connected && i < lit ? bright : dim;
        amber_draw_line(layer, layout, x - SIDE_METER_W, y, x + SIDE_METER_W,
                        y, 2.5f, color, false);
    }

}

static void draw_rpm_ring(lv_layer_t *layer, const ui_layout_t *layout,
                          const idle_screen_t *screen) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();
    const float actual = amber_progress(screen->rpm, 0.0f, RPM_MAX);
    const int lit = (int)lroundf(actual * RPM_SEGMENTS);
    const float pitch = RPM_RING_SWEEP / (float)RPM_SEGMENTS;
    const float bar = pitch - RPM_SEGMENT_GAP;

    // Trois couronnes donnent au cadran une profondeur mécanique tout en
    // conservant des extrémités nettes, comme des cellules usinées.
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, OUTER_R, 0.8f,
                   0.0f, 360.0f, separator, false);
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, OUTER_R - 4.0f, 0.55f,
                   0.0f, 360.0f, dim, false);

    for (int i = 0; i < RPM_SEGMENTS; i++) {
        const float start = RPM_RING_START + (float)i * pitch;
        const lv_color_t color = screen->connected && i < lit ? bright : dim;
        amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, RPM_RING_R,
                       RPM_RING_W, start, start + bar, color, false);
    }

    // Graduations et repères de plage : elles sont visibles à bas régime et
    // restent volontairement monochromes lorsque la liaison est absente.
    for (int i = 0; i <= 8; i++) {
        const float fraction = (float)i / 8.0f;
        const float angle = RPM_RING_START + RPM_RING_SWEEP * fraction;
        const bool major = (i % 2) == 0;
        amber_draw_tick(layer, layout, SCREEN_CX, SCREEN_CY, angle, 146.0f,
                        major ? 151.0f : 149.0f, major ? 1.4f : TICK_W,
                        screen->connected && fraction <= actual ? bright : dim,
                        false);
    }

    // Aiguille de consigne : trait fin interrompu par un point, distinct de la
    // progression réelle pour comparer instantanément cible et régime.
    const float target_angle = RPM_RING_START + RPM_RING_SWEEP *
                               amber_progress(screen->setpoint, 0.0f, RPM_MAX);
    amber_draw_tick(layer, layout, SCREEN_CX, SCREEN_CY, target_angle, 132.0f,
                    151.5f, 1.0f, separator, false);
    const float radians = target_angle * 0.01745329252f;
    amber_draw_dot(layer, layout, SCREEN_CX + cosf(radians) * 153.0f,
                   SCREEN_CY + sinf(radians) * 153.0f, 1.7f, separator);

    // Anneau de correction compact : la position autour du zéro reflète
    // l'ajusteur en tr/min sans introduire une couleur d'alerte.
    const float correction = amber_clampf(screen->adjuster, -300.0f, 300.0f);
    const float correction_span = 54.0f;
    const float correction_angle = 270.0f + correction / 300.0f * correction_span;
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, 111.0f, 1.2f,
                   270.0f - correction_span, 270.0f + correction_span, dim,
                   false);
    amber_draw_tick(layer, layout, SCREEN_CX, SCREEN_CY, correction_angle,
                    106.0f, 116.0f, 1.7f,
                    screen->connected ? bright : dim, false);
    amber_draw_dot(layer, layout, SCREEN_CX, SCREEN_CY, 2.0f, separator);
}

static void draw_panel_lines(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t separator = ui_theme_amber_separator();

    // Les repères restent ouverts : ils structurent l'information sans créer
    // de cadres qui viendraient toucher les textes ou la couronne.
    amber_draw_line(layer, layout, 58.0f, 70.0f, 116.0f, 70.0f, LINE_W,
                    separator, false);
    amber_draw_line(layer, layout, 204.0f, 70.0f, 262.0f, 70.0f, LINE_W,
                    separator, false);

    amber_draw_line(layer, layout, 56.0f, GRID_Y, 123.0f, GRID_Y, LINE_W,
                    separator, false);
    amber_draw_line(layer, layout, 197.0f, GRID_Y, 264.0f, GRID_Y, LINE_W,
                    separator, false);
    amber_draw_line(layer, layout, 128.0f, GRID_Y, 192.0f, GRID_Y, 0.55f,
                    ui_theme_amber_dim(), false);

    // Deux colonnes, deux rangées : la séparation tombe dans les espaces,
    // jamais au travers d'un nombre ou d'un libellé.
    amber_draw_line(layer, layout, 160.0f, GRID_Y + 6.0f, 160.0f, 268.0f,
                    LINE_W, separator, false);
    amber_draw_line(layer, layout, 61.0f, 232.0f, 259.0f, 232.0f, LINE_W,
                    separator, false);

    amber_draw_line(layer, layout, 67.0f, 274.0f, 253.0f, 274.0f, LINE_W,
                    separator, false);
}

static void canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    idle_screen_t *screen = lv_obj_get_user_data(canvas);
    if (layer == NULL || screen == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);

    draw_rpm_ring(layer, &layout, screen);
    draw_segment_meter(layer, &layout, SIDE_METER_X_LEFT, screen->valve,
                       0.0f, 100.0f, screen->connected);
    draw_segment_meter(layer, &layout, SIDE_METER_X_RIGHT, screen->base,
                       0.0f, 100.0f, screen->connected);
    draw_panel_lines(layer, &layout);
}

static lv_obj_t *label_create(lv_obj_t *parent, const lv_font_t *font,
                              lv_color_t color, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    if (label == NULL) return NULL;

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_label_set_text_static(label, text != NULL ? text : "");
    return label;
}

static void place_centered(lv_obj_t *object, lv_obj_t *parent, float x,
                           float y, float offset) {
    if (object == NULL) return;

    lv_obj_update_layout(object);
    const ui_layout_t layout = ui_layout_fit(lv_obj_get_width(parent),
                                             lv_obj_get_height(parent));
    const int32_t dx = px(offset * layout.scale * UI_REFERENCE_SIZE /
                          UI_DISPLAY_SIZE_PX);
    lv_obj_set_pos(object, px(ui_layout_x(&layout, x)) -
                             lv_obj_get_width(object) / 2 + dx,
                   px(ui_layout_y(&layout, y)) -
                             lv_obj_get_height(object) / 2);
}

static void set_text(lv_obj_t *label, char *buffer, size_t size,
                     const char *format, float value) {
    if (label == NULL || buffer == NULL || size == 0) return;
    if (!isfinite(value)) {
        snprintf(buffer, size, "--");
    } else {
        snprintf(buffer, size, format, value);
    }
    lv_label_set_text_static(label, buffer);
}

static void set_pair_text(lv_obj_t *front, lv_obj_t *shadow, char *buffer,
                          size_t size, const char *format, float value) {
    if (buffer == NULL || size == 0) return;
    if (!isfinite(value)) {
        snprintf(buffer, size, "--");
    } else {
        snprintf(buffer, size, format, value);
    }
    lv_label_set_text_static(front, buffer);
    lv_label_set_text_static(shadow, buffer);
}

idle_screen_t *idle_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    idle_screen_t *screen = lv_malloc(sizeof(*screen));
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
    lv_obj_set_style_bg_color(screen->root, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(screen->root, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(screen->root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(screen->root, true, 0);
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

    const lv_font_t *font_xl = ui_font_or(ui_font_xl, &lv_font_montserrat_48);
    const lv_font_t *font_value = ui_font_or(ui_font_l, &lv_font_montserrat_20);
    const lv_font_t *font_caption = ui_font_or(ui_font_m, &lv_font_montserrat_14);
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    screen->hero_text[0] = '0';
    screen->hero_text[1] = '\0';
    screen->error_text[0] = '0';
    screen->error_text[1] = '\0';
    screen->status_text[0] = 'N';
    screen->status_text[1] = 'O';
    screen->status_text[2] = ' '; 
    screen->status_text[3] = 'L';
    screen->status_text[4] = 'I';
    screen->status_text[5] = 'N';
    screen->status_text[6] = 'K';
    screen->status_text[7] = '\0';
    for (int i = 0; i < METRIC_COUNT; i++) {
        screen->metric_text[i][0] = '0';
        screen->metric_text[i][1] = '\0';
    }

    screen->title = label_create(screen->root, font_caption, bright,
                                 "RALENTI");
    screen->subtitle = label_create(screen->root, font_caption, dim,
                                    "BOUCLE FERMEE");

    // Le décalage du calque dim simule le faux-gras Michroma sans modifier la
    // police, comme sur le cadran RPM ambre de référence.
    screen->hero_shadow = label_create(screen->root, font_xl, dim,
                                       screen->hero_text);
    screen->hero = label_create(screen->root, font_xl, bright,
                                screen->hero_text);
    screen->hero_unit = label_create(screen->root, font_caption, bright, "RPM");
    screen->error = label_create(screen->root, font_value, dim,
                                 screen->error_text);
    screen->status = label_create(screen->root, font_caption, dim,
                                  screen->status_text);

    screen->metric_value[METRIC_SETPOINT] = label_create(
        screen->root, font_value, bright, screen->metric_text[METRIC_SETPOINT]);
    screen->metric_value[METRIC_VALVE] = label_create(
        screen->root, font_value, bright, screen->metric_text[METRIC_VALVE]);
    screen->metric_value[METRIC_BASE] = label_create(
        screen->root, font_value, bright, screen->metric_text[METRIC_BASE]);
    screen->metric_value[METRIC_ADJUSTER] = label_create(
        screen->root, font_value, bright, screen->metric_text[METRIC_ADJUSTER]);

    screen->metric_name[METRIC_SETPOINT] = label_create(
        screen->root, font_caption, dim, "CONSIGNE");
    screen->metric_name[METRIC_VALVE] = label_create(
        screen->root, font_caption, dim, "VANNE");
    screen->metric_name[METRIC_BASE] = label_create(
        screen->root, font_caption, dim, "BASE");
    screen->metric_name[METRIC_ADJUSTER] = label_create(
        screen->root, font_caption, dim, "AJUSTEUR");

    if (screen->title == NULL || screen->subtitle == NULL ||
        screen->hero_shadow == NULL || screen->hero == NULL ||
        screen->hero_unit == NULL || screen->error == NULL ||
        screen->status == NULL) {
        goto fail;
    }
    for (int i = 0; i < METRIC_COUNT; i++) {
        if (screen->metric_value[i] == NULL || screen->metric_name[i] == NULL) {
            goto fail;
        }
    }

    place_centered(screen->title, screen->root, SCREEN_CX, TITLE_Y, 0.0f);
    place_centered(screen->subtitle, screen->root, SCREEN_CX, SUBTITLE_Y,
                   0.0f);
    place_centered(screen->hero_shadow, screen->root, SCREEN_CX, HERO_Y,
                   BOLD_SPREAD_PX);
    place_centered(screen->hero, screen->root, SCREEN_CX, HERO_Y,
                   -BOLD_SPREAD_PX);
    place_centered(screen->hero_unit, screen->root, SCREEN_CX, HERO_UNIT_Y,
                   0.0f);
    place_centered(screen->error, screen->root, SCREEN_CX, ERROR_Y, 0.0f);
    place_centered(screen->status, screen->root, SCREEN_CX, STATUS_Y, 0.0f);
    for (int i = 0; i < METRIC_COUNT; i++) {
        place_centered(screen->metric_value[i], screen->root, kMetricX[i],
                       kMetricValueY[i], 0.0f);
        place_centered(screen->metric_name[i], screen->root, kMetricX[i],
                       kMetricNameY[i], 0.0f);
    }

    return screen;

fail:
    idle_screen_destroy(screen);
    return NULL;
}

void idle_screen_update(idle_screen_t *screen, const ecu_data_t *data) {
    if (screen == NULL || data == NULL || screen->canvas == NULL) return;

    screen->rpm = data->rpm;
    screen->setpoint = data->idle_setpoint;
    screen->valve = data->idle_valve_position;
    screen->base = data->idle_base_position;
    screen->error_value = data->idle_error;
    screen->adjuster = data->idle_adjuster_rpm;
    screen->connected = data->connected;

    set_pair_text(screen->hero, screen->hero_shadow, screen->hero_text,
                  sizeof(screen->hero_text), "%.0f", screen->rpm);
    set_text(screen->error, screen->error_text, sizeof(screen->error_text),
             "ERREUR %+.0f RPM", screen->error_value);
    snprintf(screen->status_text, sizeof(screen->status_text),
             data->connected ? "BOUCLE ACTIVE" : "PAS DE LIEN");
    lv_label_set_text_static(screen->status, screen->status_text);

    set_text(screen->metric_value[METRIC_SETPOINT],
             screen->metric_text[METRIC_SETPOINT],
             sizeof(screen->metric_text[METRIC_SETPOINT]), "%.0f",
             screen->setpoint);
    set_text(screen->metric_value[METRIC_VALVE],
             screen->metric_text[METRIC_VALVE],
             sizeof(screen->metric_text[METRIC_VALVE]), "%.0f%%",
             screen->valve);
    set_text(screen->metric_value[METRIC_BASE],
             screen->metric_text[METRIC_BASE],
             sizeof(screen->metric_text[METRIC_BASE]), "%.0f%%",
             screen->base);
    set_text(screen->metric_value[METRIC_ADJUSTER],
             screen->metric_text[METRIC_ADJUSTER],
             sizeof(screen->metric_text[METRIC_ADJUSTER]), "%+.0f",
             screen->adjuster);

    // Le texte à largeur intrinsèque est recentré sans recréer de widget.
    place_centered(screen->hero_shadow, screen->root, SCREEN_CX, HERO_Y,
                   BOLD_SPREAD_PX);
    place_centered(screen->hero, screen->root, SCREEN_CX, HERO_Y,
                   -BOLD_SPREAD_PX);
    place_centered(screen->error, screen->root, SCREEN_CX, ERROR_Y, 0.0f);
    place_centered(screen->status, screen->root, SCREEN_CX, STATUS_Y, 0.0f);
    for (int i = 0; i < METRIC_COUNT; i++) {
        place_centered(screen->metric_value[i], screen->root, kMetricX[i],
                       kMetricValueY[i], 0.0f);
    }
    lv_obj_invalidate(screen->canvas);
}

void idle_screen_destroy(idle_screen_t *screen) {
    if (screen == NULL) return;
    if (screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
