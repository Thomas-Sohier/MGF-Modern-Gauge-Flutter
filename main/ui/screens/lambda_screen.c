#include "ui/screens/lambda_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>

// Repère unique du cadran ambre : 320 unités, quelle que soit la taille du
// parent. La composition reste à l'intérieur d'une marge circulaire sûre.
#define CX 160.0f
#define CY 160.0f

#define AFR_MIN 10.0f
#define AFR_MAX 20.0f
#define AFR_SEGMENTS 12
#define AFR_BAR_X1 66.0f
#define AFR_BAR_X2 254.0f
#define AFR_BAR_Y 70.0f
#define AFR_BAR_GAP 5.0f
#define AFR_BAR_W 6.0f

#define O2_SEGMENTS 5
#define O2_BAR_GAP 4.0f
#define O2_BAR_W 4.5f
#define O2_LEFT_X1 48.0f
#define O2_LEFT_X2 112.0f
#define O2_RIGHT_X1 208.0f
#define O2_RIGHT_X2 272.0f
#define O2_BAR_Y 253.0f

// Une paire de labels permet de conserver le faux-gras du style amber sans
// dépendre d'une variante bold de Michroma. Les buffers appartiennent à la
// vue et vivent aussi longtemps que les labels.
typedef struct {
    lv_obj_t *shadow;
    lv_obj_t *front;
} amber_text_t;

struct lambda_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;

    amber_text_t title;
    amber_text_t afr;
    amber_text_t afr_unit;
    amber_text_t lambda_value;
    amber_text_t lambda_unit;
    amber_text_t lambda_label;
    amber_text_t o2_value;
    amber_text_t o2_unit;
    amber_text_t o2_label;
    amber_text_t duty_value;
    amber_text_t duty_label;

    char afr_buf[12];
    char lambda_buf[12];
    char o2_buf[12];
    char duty_buf[12];
    char duty_state_buf[28];
    bool connected;
    float afr_value;
    float lambda_mv;
    float o2_mv;
    float duty;
};

static void draw_segment_bar(lv_layer_t *layer, const ui_layout_t *layout,
                             float x1, float x2, float y, int segments,
                             float gap, float width, int lit, bool connected) {
    const float step = (x2 - x1) / (float)segments;
    const float segment_width = step - gap;
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    for (int i = 0; i < segments; i++) {
        const float start = x1 + i * step + gap * 0.5f;
        const float end = start + segment_width;
        const lv_color_t color = connected && i < lit ? bright : dim;
        amber_draw_line(layer, layout, start, y, end, y, width, color, false);
    }
}

static void draw_afr_meter(lv_layer_t *layer, const ui_layout_t *layout,
                           const lambda_screen_t *screen) {
    const int lit = (int)lroundf(amber_progress(screen->afr_value, AFR_MIN,
                                                AFR_MAX) * AFR_SEGMENTS);
    draw_segment_bar(layer, layout, AFR_BAR_X1, AFR_BAR_X2, AFR_BAR_Y,
                     AFR_SEGMENTS, AFR_BAR_GAP, AFR_BAR_W, lit,
                     screen->connected);
}

static void draw_o2_meter(lv_layer_t *layer, const ui_layout_t *layout,
                          float x1, float x2, float value, bool connected) {
    const int lit = (int)lroundf(amber_progress(value, 0.0f, 1000.0f) *
                               O2_SEGMENTS);
    draw_segment_bar(layer, layout, x1, x2, O2_BAR_Y, O2_SEGMENTS,
                     O2_BAR_GAP, O2_BAR_W, lit, connected);
}

static void canvas_draw_cb(lv_event_t *event) {
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lambda_screen_t *screen = lv_obj_get_user_data(canvas);
    if (layer == NULL || screen == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);

    // Une seule lecture graphique par niveau : un indicateur AFR dominant et
    // deux petites barres de tension, sans couronne ni graduation redondante.
    draw_afr_meter(layer, &layout, screen);
    draw_o2_meter(layer, &layout, O2_LEFT_X1, O2_LEFT_X2,
                  screen->lambda_mv, screen->connected);
    draw_o2_meter(layer, &layout, O2_RIGHT_X1, O2_RIGHT_X2,
                  screen->o2_mv, screen->connected);
}

static void text_style(amber_text_t *text, lv_color_t front,
                       lv_color_t shadow) {
    lv_obj_set_style_text_color(text->front, front, 0);
    lv_obj_set_style_text_color(text->shadow, shadow, 0);
}

lambda_screen_t *lambda_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    lambda_screen_t *screen = lv_malloc(sizeof(*screen));
    if (screen == NULL) return NULL;
    lv_memzero(screen, sizeof(*screen));

    lv_obj_set_style_bg_color(parent, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_update_layout(parent);

    screen->root = amber_ui_root_create(parent);
    if (screen->root == NULL) goto fail;
    screen->canvas = amber_ui_canvas_create(screen->root, screen,
                                            canvas_draw_cb);
    if (screen->canvas == NULL) goto fail;

    const lv_font_t *title_font = ui_font_or(ui_font_m, &lv_font_montserrat_20);
    const lv_font_t *body_font = amber_ui_font_value();
    const lv_font_t *caption_font = amber_ui_font_caption();
    const lv_font_t *hero_font = amber_ui_font_hero();
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();

    screen->title.shadow = amber_ui_label_create(
        screen->root, title_font, separator, "AFR", 120.0f);
    screen->title.front = amber_ui_label_create(
        screen->root, title_font, bright, "AFR", 120.0f);
    amber_ui_place_centered(screen->title.shadow, screen->root, CX, 29.0f,
                            120.0f, amber_ui_bold_spread(1));
    amber_ui_place_centered(screen->title.front, screen->root, CX, 29.0f,
                            120.0f, -amber_ui_bold_spread(1));

    screen->afr.shadow = amber_ui_label_create(
        screen->root, hero_font, separator, "--.--", 150.0f);
    screen->afr.front = amber_ui_label_create(
        screen->root, hero_font, bright, "--.--", 150.0f);
    amber_ui_place_centered(screen->afr.shadow, screen->root, CX, 130.0f,
                            150.0f, amber_ui_bold_spread(2));
    amber_ui_place_centered(screen->afr.front, screen->root, CX, 130.0f,
                            150.0f, -amber_ui_bold_spread(2));

    screen->afr_unit.shadow = amber_ui_label_create(
        screen->root, body_font, separator, "AIR / CARBURANT", 220.0f);
    screen->afr_unit.front = amber_ui_label_create(
        screen->root, body_font, bright, "AIR / CARBURANT", 220.0f);
    amber_ui_place_centered(screen->afr_unit.shadow, screen->root, CX, 166.0f,
                            220.0f, amber_ui_bold_spread(1));
    amber_ui_place_centered(screen->afr_unit.front, screen->root, CX, 166.0f,
                            220.0f, -amber_ui_bold_spread(1));

    screen->lambda_label.shadow = amber_ui_label_create(
        screen->root, caption_font, dim, "O2 GAUCHE", 112.0f);
    screen->lambda_label.front = amber_ui_label_create(
        screen->root, caption_font, dim, "O2 GAUCHE", 112.0f);
    amber_ui_place_centered(screen->lambda_label.shadow, screen->root, 80.0f,
                            202.0f, 112.0f, 0.0f);
    amber_ui_place_centered(screen->lambda_label.front, screen->root, 80.0f,
                            202.0f, 112.0f, 0.0f);

    screen->o2_label.shadow = amber_ui_label_create(
        screen->root, caption_font, dim, "O2 DROITE", 112.0f);
    screen->o2_label.front = amber_ui_label_create(
        screen->root, caption_font, dim, "O2 DROITE", 112.0f);
    amber_ui_place_centered(screen->o2_label.shadow, screen->root, 240.0f,
                            202.0f, 112.0f, 0.0f);
    amber_ui_place_centered(screen->o2_label.front, screen->root, 240.0f,
                            202.0f, 112.0f, 0.0f);

    screen->lambda_value.shadow = amber_ui_label_create(
        screen->root, body_font, separator, "--", 100.0f);
    screen->lambda_value.front = amber_ui_label_create(
        screen->root, body_font, bright, "--", 100.0f);
    amber_ui_place_centered(screen->lambda_value.shadow, screen->root, 80.0f,
                            222.0f, 100.0f, amber_ui_bold_spread(1));
    amber_ui_place_centered(screen->lambda_value.front, screen->root, 80.0f,
                            222.0f, 100.0f, -amber_ui_bold_spread(1));

    screen->o2_value.shadow = amber_ui_label_create(
        screen->root, body_font, separator, "--", 100.0f);
    screen->o2_value.front = amber_ui_label_create(
        screen->root, body_font, bright, "--", 100.0f);
    amber_ui_place_centered(screen->o2_value.shadow, screen->root, 240.0f,
                            222.0f, 100.0f, amber_ui_bold_spread(1));
    amber_ui_place_centered(screen->o2_value.front, screen->root, 240.0f,
                            222.0f, 100.0f, -amber_ui_bold_spread(1));

    screen->lambda_unit.shadow = amber_ui_label_create(
        screen->root, caption_font, dim, "mV", 48.0f);
    screen->lambda_unit.front = amber_ui_label_create(
        screen->root, caption_font, dim, "mV", 48.0f);
    amber_ui_place_centered(screen->lambda_unit.shadow, screen->root, 80.0f,
                            240.0f, 48.0f, 0.0f);
    amber_ui_place_centered(screen->lambda_unit.front, screen->root, 80.0f,
                            240.0f, 48.0f, 0.0f);

    screen->o2_unit.shadow = amber_ui_label_create(
        screen->root, caption_font, dim, "mV", 48.0f);
    screen->o2_unit.front = amber_ui_label_create(
        screen->root, caption_font, dim, "mV", 48.0f);
    amber_ui_place_centered(screen->o2_unit.shadow, screen->root, 240.0f,
                            240.0f, 48.0f, 0.0f);
    amber_ui_place_centered(screen->o2_unit.front, screen->root, 240.0f,
                            240.0f, 48.0f, 0.0f);

    screen->duty_value.shadow = amber_ui_label_create(
        screen->root, body_font, separator, "--%", 100.0f);
    screen->duty_value.front = amber_ui_label_create(
        screen->root, body_font, bright, "--%", 100.0f);
    amber_ui_place_centered(screen->duty_value.shadow, screen->root, CX, 269.0f,
                            100.0f, amber_ui_bold_spread(1));
    amber_ui_place_centered(screen->duty_value.front, screen->root, CX, 269.0f,
                            100.0f, -amber_ui_bold_spread(1));

    screen->duty_label.shadow = amber_ui_label_create(
        screen->root, caption_font, separator, "CHAUFFAGE INACTIF", 200.0f);
    screen->duty_label.front = amber_ui_label_create(
        screen->root, caption_font, dim, "CHAUFFAGE INACTIF", 200.0f);
    amber_ui_place_centered(screen->duty_label.shadow, screen->root, CX, 291.0f,
                            200.0f, 0.0f);
    amber_ui_place_centered(screen->duty_label.front, screen->root, CX, 291.0f,
                            200.0f, 0.0f);

    if (screen->title.shadow == NULL || screen->title.front == NULL ||
        screen->afr.shadow == NULL || screen->afr.front == NULL ||
        screen->afr_unit.shadow == NULL || screen->afr_unit.front == NULL ||
        screen->lambda_value.shadow == NULL ||
        screen->lambda_value.front == NULL ||
        screen->lambda_unit.shadow == NULL || screen->lambda_unit.front == NULL ||
        screen->lambda_label.shadow == NULL ||
        screen->lambda_label.front == NULL ||
        screen->o2_value.shadow == NULL || screen->o2_value.front == NULL ||
        screen->o2_unit.shadow == NULL || screen->o2_unit.front == NULL ||
        screen->o2_label.shadow == NULL || screen->o2_label.front == NULL ||
        screen->duty_value.shadow == NULL || screen->duty_value.front == NULL ||
        screen->duty_label.shadow == NULL || screen->duty_label.front == NULL) {
        goto fail;
    }

    // Le décalage de deux couleurs faisait apparaître une seconde glyph plutôt
    // qu'une graisse homogène. Sur cet écran, les labels restent donc sur un
    // seul calque net ; les objets shadow sont conservés pour garder l'API de
    // mise à jour simple et pourront disparaître lors d'un refactoring global.
    amber_text_t *const texts[] = {
        &screen->title,       &screen->afr,          &screen->afr_unit,
        &screen->lambda_value, &screen->lambda_unit, &screen->lambda_label,
        &screen->o2_value,    &screen->o2_unit,      &screen->o2_label,
        &screen->duty_value,  &screen->duty_label,
    };
    for (size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); i++) {
        lv_obj_add_flag(texts[i]->shadow, LV_OBJ_FLAG_HIDDEN);
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
                            ? amber_clampf(data->estimated_air_fuel, AFR_MIN,
                                           AFR_MAX)
                            : AFR_MIN;
    screen->lambda_mv = isfinite(data->lambda_mv)
                            ? amber_clampf(data->lambda_mv, 0.0f, 1000.0f)
                            : 0.0f;
    screen->o2_mv = isfinite(data->o2_mv)
                        ? amber_clampf(data->o2_mv, 0.0f, 1000.0f)
                        : 0.0f;
    screen->duty = isfinite(data->lambda_sensor_duty_cycle)
                       ? amber_clampf(data->lambda_sensor_duty_cycle, 0.0f,
                                      100.0f)
                       : 0.0f;

    if (screen->connected) {
        snprintf(screen->afr_buf, sizeof(screen->afr_buf), "%.2f",
                 screen->afr_value);
        snprintf(screen->lambda_buf, sizeof(screen->lambda_buf), "%.0f",
                 screen->lambda_mv);
        snprintf(screen->o2_buf, sizeof(screen->o2_buf), "%.0f", screen->o2_mv);
        snprintf(screen->duty_buf, sizeof(screen->duty_buf), "%.0f%%",
                 screen->duty);
    } else {
        snprintf(screen->afr_buf, sizeof(screen->afr_buf), "--.--");
        snprintf(screen->lambda_buf, sizeof(screen->lambda_buf), "--");
        snprintf(screen->o2_buf, sizeof(screen->o2_buf), "--");
        snprintf(screen->duty_buf, sizeof(screen->duty_buf), "--%%");
    }

    const char *heater_state = !screen->connected
                                   ? "CHAUFFAGE INDISPONIBLE"
                               : screen->duty > 0.0f
                                   ? "CHAUFFAGE ACTIF"
                                   : "CHAUFFAGE INACTIF";
    snprintf(screen->duty_state_buf, sizeof(screen->duty_state_buf), "%s",
             heater_state);

    lv_label_set_text_static(screen->afr.shadow, screen->afr_buf);
    lv_label_set_text_static(screen->afr.front, screen->afr_buf);
    amber_ui_place_centered(screen->afr.shadow, screen->root, CX, 130.0f,
                            150.0f, amber_ui_bold_spread(2));
    amber_ui_place_centered(screen->afr.front, screen->root, CX, 130.0f,
                            150.0f, -amber_ui_bold_spread(2));

    lv_label_set_text_static(screen->lambda_value.shadow, screen->lambda_buf);
    lv_label_set_text_static(screen->lambda_value.front, screen->lambda_buf);
    amber_ui_place_centered(screen->lambda_value.shadow, screen->root, 80.0f,
                            222.0f, 100.0f, amber_ui_bold_spread(1));
    amber_ui_place_centered(screen->lambda_value.front, screen->root, 80.0f,
                            222.0f, 100.0f, -amber_ui_bold_spread(1));

    lv_label_set_text_static(screen->o2_value.shadow, screen->o2_buf);
    lv_label_set_text_static(screen->o2_value.front, screen->o2_buf);
    amber_ui_place_centered(screen->o2_value.shadow, screen->root, 240.0f,
                            222.0f, 100.0f, amber_ui_bold_spread(1));
    amber_ui_place_centered(screen->o2_value.front, screen->root, 240.0f,
                            222.0f, 100.0f, -amber_ui_bold_spread(1));

    lv_label_set_text_static(screen->duty_value.shadow, screen->duty_buf);
    lv_label_set_text_static(screen->duty_value.front, screen->duty_buf);
    amber_ui_place_centered(screen->duty_value.shadow, screen->root, CX, 269.0f,
                            100.0f, amber_ui_bold_spread(1));
    amber_ui_place_centered(screen->duty_value.front, screen->root, CX, 269.0f,
                            100.0f, -amber_ui_bold_spread(1));

    lv_label_set_text_static(screen->duty_label.shadow, screen->duty_state_buf);
    lv_label_set_text_static(screen->duty_label.front, screen->duty_state_buf);
    amber_ui_place_centered(screen->duty_label.shadow, screen->root, CX, 291.0f,
                            200.0f, 0.0f);
    amber_ui_place_centered(screen->duty_label.front, screen->root, CX, 291.0f,
                            200.0f, 0.0f);
    text_style(&screen->duty_label,
               screen->connected && screen->duty > 0.0f
                   ? ui_theme_amber_bright()
                   : ui_theme_amber_dim(),
               ui_theme_amber_separator());

    lv_obj_invalidate(screen->canvas);
}

void lambda_screen_destroy(lambda_screen_t *screen) {
    if (screen == NULL) return;

    if (screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
