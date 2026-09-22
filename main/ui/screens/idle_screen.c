#include "ui/screens/idle_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>

// Toutes les coordonnées sont dans le cadran de conception 320 x 320. La
// translation et l'échelle sont calculées à partir de la zone réellement
// disponible, sans hypothèse sur le parent.
#define SCREEN_CX 160.0f
#define SCREEN_CY 160.0f

#define RPM_RING_R 141.0f
#define RPM_RING_W 7.0f
#define RPM_MAX 2000.0f
#define RPM_RING_START 135.0f
#define RPM_RING_SWEEP 270.0f
#define RPM_SEGMENTS 28
#define RPM_SEGMENT_GAP 1.8f

// La couronne reste en retrait du bord réel du panneau. Le centre est réservé
// à la lecture immédiate : RPM, erreur, puis les quatre valeurs de régulation.
#define TITLE_Y 35.0f
#define SUBTITLE_Y 61.0f
#define HERO_Y 116.0f
#define HERO_UNIT_Y 151.0f
#define ERROR_Y 175.0f
#define GRID_Y 238.0f
#define METRIC_VALUE_TOP_Y 204.0f
#define METRIC_NAME_TOP_Y 226.0f
#define METRIC_VALUE_BOTTOM_Y 253.0f
#define METRIC_NAME_BOTTOM_Y 276.0f
#define STATUS_Y 296.0f

#define LINE_W 0.8f
#define BOLD_SPREAD_PX 2
// LVGL exprime l'échelle de transformation avec 256 = 100 %.
#define HERO_SCALE 288
#define METRIC_SCALE 320

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

static void draw_rpm_ring(lv_layer_t *layer, const ui_layout_t *layout,
                          const idle_screen_t *screen) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const float actual = amber_progress(screen->rpm, 0.0f, RPM_MAX);
    const int lit = (int)lroundf(actual * RPM_SEGMENTS);
    const float pitch = RPM_RING_SWEEP / (float)RPM_SEGMENTS;
    const float bar = pitch - RPM_SEGMENT_GAP;

    // Une seule couronne segmentée suffit à situer le régime. Les graduations
    // latérales, l'aiguille et l'arc intérieur sont volontairement absents :
    // ils détournaient l'œil des chiffres utiles à la conduite.
    for (int i = 0; i < RPM_SEGMENTS; i++) {
        const float start = RPM_RING_START + (float)i * pitch;
        const lv_color_t color = screen->connected && i < lit ? bright : dim;
        amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, RPM_RING_R,
                       RPM_RING_W, start, start + bar, color, false);
    }
}

static void draw_panel_lines(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t separator = ui_theme_amber_separator();

    // Une croix ouverte suffit à lire les quatre cases. Elle reste dans la
    // zone sûre du disque et ne ferme aucun cadre autour des textes.
    amber_draw_line(layer, layout, 160.0f, 194.0f, 160.0f, 282.0f, LINE_W,
                    separator, false);
    amber_draw_line(layer, layout, 64.0f, GRID_Y, 256.0f, GRID_Y, LINE_W,
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
    draw_panel_lines(layer, &layout);
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

    screen->root = amber_ui_root_create(parent);
    if (screen->root == NULL) goto fail;
    screen->canvas = amber_ui_canvas_create(screen->root, screen,
                                            canvas_draw_cb);
    if (screen->canvas == NULL) goto fail;

    const lv_font_t *font_xl = amber_ui_font_hero();
    const lv_font_t *font_value = amber_ui_font_value();
    const lv_font_t *font_caption = amber_ui_font_caption();
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

    screen->title = amber_ui_label_create(screen->root, font_caption, bright,
                                          "RALENTI", 0.0f);
    screen->subtitle = amber_ui_label_create(screen->root, font_caption, dim,
                                             "", 0.0f);

    // Le décalage du calque dim simule le faux-gras Michroma sans modifier la
    // police, comme sur le cadran RPM ambre de référence.
    screen->hero_shadow = amber_ui_label_create(screen->root, font_xl, dim,
                                                screen->hero_text, 0.0f);
    screen->hero = amber_ui_label_create(screen->root, font_xl, bright,
                                         screen->hero_text, 0.0f);
    screen->hero_unit = amber_ui_label_create(screen->root, font_caption,
                                              bright, "RPM", 0.0f);
    screen->error = amber_ui_label_create(screen->root, font_value, bright,
                                          screen->error_text, 0.0f);
    screen->status = amber_ui_label_create(screen->root, font_caption, dim,
                                           screen->status_text, 0.0f);

    screen->metric_value[METRIC_SETPOINT] = amber_ui_label_create(
        screen->root, font_value, bright, screen->metric_text[METRIC_SETPOINT],
        0.0f);
    screen->metric_value[METRIC_VALVE] = amber_ui_label_create(
        screen->root, font_value, bright, screen->metric_text[METRIC_VALVE],
        0.0f);
    screen->metric_value[METRIC_BASE] = amber_ui_label_create(
        screen->root, font_value, bright, screen->metric_text[METRIC_BASE],
        0.0f);
    screen->metric_value[METRIC_ADJUSTER] = amber_ui_label_create(
        screen->root, font_value, bright, screen->metric_text[METRIC_ADJUSTER],
        0.0f);

    screen->metric_name[METRIC_SETPOINT] = amber_ui_label_create(
        screen->root, font_caption, dim, "CONSIGNE", 0.0f);
    screen->metric_name[METRIC_VALVE] = amber_ui_label_create(
        screen->root, font_caption, dim, "VANNE", 0.0f);
    screen->metric_name[METRIC_BASE] = amber_ui_label_create(
        screen->root, font_caption, dim, "BASE", 0.0f);
    screen->metric_name[METRIC_ADJUSTER] = amber_ui_label_create(
        screen->root, font_caption, dim, "AJUSTEUR", 0.0f);

    if (screen->title == NULL || screen->subtitle == NULL ||
        screen->hero_shadow == NULL || screen->hero == NULL ||
        screen->hero_unit == NULL || screen->error == NULL ||
        screen->status == NULL) {
        goto fail;
    }
    lv_obj_add_flag(screen->hero_shadow, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < METRIC_COUNT; i++) {
        if (screen->metric_value[i] == NULL || screen->metric_name[i] == NULL) {
            goto fail;
        }
    }

    lv_obj_set_style_transform_scale(screen->hero_shadow, HERO_SCALE, 0);
    lv_obj_set_style_transform_scale(screen->hero, HERO_SCALE, 0);
    lv_obj_set_style_transform_scale(screen->error, HERO_SCALE, 0);
    for (int i = 0; i < METRIC_COUNT; i++) {
        lv_obj_set_style_transform_scale(screen->metric_value[i], METRIC_SCALE, 0);
    }

    amber_ui_place_centered(screen->title, screen->root, SCREEN_CX, TITLE_Y,
                             0.0f, 0.0f);
    amber_ui_place_centered(screen->subtitle, screen->root, SCREEN_CX,
                             SUBTITLE_Y, 0.0f, 0.0f);
    amber_ui_place_centered(screen->hero_shadow, screen->root, SCREEN_CX,
                             HERO_Y, 0.0f,
                             BOLD_SPREAD_PX * UI_REFERENCE_SIZE /
                             UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(screen->hero, screen->root, SCREEN_CX, HERO_Y,
                             0.0f,
                             -BOLD_SPREAD_PX * UI_REFERENCE_SIZE /
                             UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(screen->hero_unit, screen->root, SCREEN_CX,
                             HERO_UNIT_Y, 0.0f, 0.0f);
    amber_ui_place_centered(screen->error, screen->root, SCREEN_CX, ERROR_Y,
                             0.0f, 0.0f);
    amber_ui_place_centered(screen->status, screen->root, SCREEN_CX, STATUS_Y,
                             0.0f, 0.0f);
    for (int i = 0; i < METRIC_COUNT; i++) {
        amber_ui_place_centered(screen->metric_value[i], screen->root,
                                kMetricX[i], kMetricValueY[i], 0.0f, 0.0f);
        amber_ui_place_centered(screen->metric_name[i], screen->root,
                                kMetricX[i], kMetricNameY[i], 0.0f, 0.0f);
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
    amber_ui_place_centered(screen->hero_shadow, screen->root, SCREEN_CX,
                             HERO_Y, 0.0f,
                             BOLD_SPREAD_PX * UI_REFERENCE_SIZE /
                             UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(screen->hero, screen->root, SCREEN_CX, HERO_Y,
                             0.0f,
                             -BOLD_SPREAD_PX * UI_REFERENCE_SIZE /
                             UI_DISPLAY_SIZE_PX);
    amber_ui_place_centered(screen->error, screen->root, SCREEN_CX, ERROR_Y,
                             0.0f, 0.0f);
    amber_ui_place_centered(screen->status, screen->root, SCREEN_CX, STATUS_Y,
                             0.0f, 0.0f);
    for (int i = 0; i < METRIC_COUNT; i++) {
        amber_ui_place_centered(screen->metric_value[i], screen->root,
                                kMetricX[i], kMetricValueY[i], 0.0f, 0.0f);
    }
    lv_obj_invalidate(screen->canvas);
}

void idle_screen_destroy(idle_screen_t *screen) {
    if (screen == NULL) return;
    if (screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
