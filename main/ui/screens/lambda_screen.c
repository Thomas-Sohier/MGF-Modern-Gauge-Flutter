#include "ui/screens/lambda_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

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
    bool has_snapshot;
    int32_t afr_lit;
    int32_t lambda_lit;
    int32_t o2_lit;
    bool heater_active;
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

static bool set_text_if_changed(amber_text_t *text, char *buffer,
                                size_t buffer_size, const char *value) {
    if (strcmp(buffer, value) == 0) return false;
    snprintf(buffer, buffer_size, "%s", value);
    lv_label_set_text_static(text->shadow, buffer);
    lv_label_set_text_static(text->front, buffer);
    return true;
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

    const bool connected = data->connected;
    const float afr = isfinite(data->estimated_air_fuel)
        ? amber_clampf(data->estimated_air_fuel, AFR_MIN, AFR_MAX) : AFR_MIN;
    const float lambda_mv = isfinite(data->lambda_mv)
        ? amber_clampf(data->lambda_mv, 0.0f, 1000.0f) : 0.0f;
    const float o2_mv = isfinite(data->o2_mv)
        ? amber_clampf(data->o2_mv, 0.0f, 1000.0f) : 0.0f;
    const float duty = isfinite(data->lambda_sensor_duty_cycle)
        ? amber_clampf(data->lambda_sensor_duty_cycle, 0.0f, 100.0f) : 0.0f;
    const int32_t afr_lit = (int32_t)lroundf(
        amber_progress(afr, AFR_MIN, AFR_MAX) * AFR_SEGMENTS);
    const int32_t lambda_lit = (int32_t)lroundf(
        amber_progress(lambda_mv, 0.0f, 1000.0f) * O2_SEGMENTS);
    const int32_t o2_lit = (int32_t)lroundf(
        amber_progress(o2_mv, 0.0f, 1000.0f) * O2_SEGMENTS);
    const bool bars_changed = !screen->has_snapshot ||
        connected != screen->connected || afr_lit != screen->afr_lit ||
        lambda_lit != screen->lambda_lit || o2_lit != screen->o2_lit;
    const bool heater_active = connected && duty > 0.0f;
    const bool style_changed = !screen->has_snapshot ||
                               heater_active != screen->heater_active;

    char next_afr[12], next_lambda[12], next_o2[12], next_duty[12];
    if (connected) {
        snprintf(next_afr, sizeof(next_afr), "%.2f", afr);
        snprintf(next_lambda, sizeof(next_lambda), "%.0f", lambda_mv);
        snprintf(next_o2, sizeof(next_o2), "%.0f", o2_mv);
        if (isfinite(data->lambda_sensor_duty_cycle)) {
            snprintf(next_duty, sizeof(next_duty), "%.0f%%", duty);
        } else {
            snprintf(next_duty, sizeof(next_duty), "--%%");
        }
    } else {
        snprintf(next_afr, sizeof(next_afr), "--.--");
        snprintf(next_lambda, sizeof(next_lambda), "--");
        snprintf(next_o2, sizeof(next_o2), "--");
        snprintf(next_duty, sizeof(next_duty), "--%%");
    }
    const char *heater_state = !connected ? "CHAUFFAGE INDISPONIBLE"
        : duty > 0.0f ? "CHAUFFAGE ACTIF" : "CHAUFFAGE INACTIF";

    const bool afr_changed = set_text_if_changed(&screen->afr, screen->afr_buf,
                                                  sizeof(screen->afr_buf), next_afr);
    const bool lambda_changed = set_text_if_changed(&screen->lambda_value,
        screen->lambda_buf, sizeof(screen->lambda_buf), next_lambda);
    const bool o2_changed = set_text_if_changed(&screen->o2_value,
        screen->o2_buf, sizeof(screen->o2_buf), next_o2);
    const bool duty_changed = set_text_if_changed(&screen->duty_value,
        screen->duty_buf, sizeof(screen->duty_buf), next_duty);
    const bool state_changed = set_text_if_changed(&screen->duty_label,
        screen->duty_state_buf, sizeof(screen->duty_state_buf), heater_state);

    screen->connected = connected;
    screen->afr_value = afr;
    screen->lambda_mv = lambda_mv;
    screen->o2_mv = o2_mv;
    screen->duty = duty;
    screen->afr_lit = afr_lit;
    screen->lambda_lit = lambda_lit;
    screen->o2_lit = o2_lit;
    screen->heater_active = heater_active;
    screen->has_snapshot = true;

    if (afr_changed) {
        amber_ui_place_centered(screen->afr.shadow, screen->root, CX, 130.0f,
                                150.0f, amber_ui_bold_spread(2));
        amber_ui_place_centered(screen->afr.front, screen->root, CX, 130.0f,
                                150.0f, -amber_ui_bold_spread(2));
    }
    if (lambda_changed) {
        amber_ui_place_centered(screen->lambda_value.shadow, screen->root, 80.0f,
                                222.0f, 100.0f, amber_ui_bold_spread(1));
        amber_ui_place_centered(screen->lambda_value.front, screen->root, 80.0f,
                                222.0f, 100.0f, -amber_ui_bold_spread(1));
    }
    if (o2_changed) {
        amber_ui_place_centered(screen->o2_value.shadow, screen->root, 240.0f,
                                222.0f, 100.0f, amber_ui_bold_spread(1));
        amber_ui_place_centered(screen->o2_value.front, screen->root, 240.0f,
                                222.0f, 100.0f, -amber_ui_bold_spread(1));
    }
    if (duty_changed) {
        amber_ui_place_centered(screen->duty_value.shadow, screen->root, CX, 269.0f,
                                100.0f, amber_ui_bold_spread(1));
        amber_ui_place_centered(screen->duty_value.front, screen->root, CX, 269.0f,
                                100.0f, -amber_ui_bold_spread(1));
    }
    if (state_changed || style_changed) {
        if (state_changed) {
            amber_ui_place_centered(screen->duty_label.shadow, screen->root, CX,
                                    291.0f, 200.0f, 0.0f);
            amber_ui_place_centered(screen->duty_label.front, screen->root, CX,
                                    291.0f, 200.0f, 0.0f);
        }
        if (style_changed) {
            text_style(&screen->duty_label,
                       heater_active ? ui_theme_amber_bright()
                                     : ui_theme_amber_dim(),
                       ui_theme_amber_separator());
        }
    }
    if (bars_changed) lv_obj_invalidate(screen->canvas);
}

void lambda_screen_destroy(lambda_screen_t *screen) {
    if (screen == NULL) return;

    if (screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
