#include "ui/screens/ignition_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Toutes les coordonnées sont exprimées dans le repère ambre partagé 320 x
// 320. Le contenu est ensuite inscrit dans le plus grand carré disponible.
#define SCREEN_CX 160.0f
#define SCREEN_CY 160.0f
#define ADVANCE_MIN       (-10.0f)
#define ADVANCE_MAX       50.0f
#define ADVANCE_START     210.0f
#define ADVANCE_SWEEP     120.0f
#define ADVANCE_RADIUS    142.0f
#define ADVANCE_TICKS     7

#define TICK_W             1.3f
#define BOLD_SPREAD_PX     1
#define METRIC_SCALE       288

// Grille 2x2 inscrite dans le disque : les valeurs disposent d'une gouttière
// centrale généreuse et la dernière ligne reste au-dessus de la zone courbe.
static const float k_metric_x[4] = {100.0f, 220.0f, 100.0f, 220.0f};
static const float k_metric_value_y[4] = {225.0f, 225.0f, 263.0f, 263.0f};
static const float k_metric_name_y[4] = {246.0f, 246.0f, 285.0f, 285.0f};

struct ignition_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;

    lv_obj_t *advance_shadow;
    lv_obj_t *advance;
    lv_obj_t *advance_caption;
    lv_obj_t *metric_shadow[4];
    lv_obj_t *metric_value[4];
    lv_obj_t *metric_name[4];

    // Les labels dynamiques pointent toujours vers ces buffers. Ainsi
    // lv_label_set_text_static() ne crée rien pendant une mise à jour ECU.
    char advance_text[16];
    char offset_text[20];
    char coil_1_text[20];
    char coil_2_text[20];
    char coil_total_text[20];

    float advance_value;
    bool has_snapshot;
    int32_t advance_bucket;
};


static void draw_spark_symbol(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t separator = ui_theme_amber_separator();

    // Éclair réduit à sa silhouette : un seul pictogramme, sans rayons ni
    // détail concurrent de la valeur centrale.
    amber_draw_line(layer, layout, 154.0f, 92.0f, 165.0f, 92.0f, 2.6f,
                    separator, true);
    amber_draw_line(layer, layout, 165.0f, 92.0f, 157.0f, 102.0f, 2.6f,
                    separator, true);
    amber_draw_line(layer, layout, 157.0f, 102.0f, 166.0f, 102.0f, 2.6f,
                    separator, true);
    amber_draw_line(layer, layout, 166.0f, 102.0f, 153.0f, 116.0f, 2.6f,
                    separator, true);

    amber_draw_line(layer, layout, 154.0f, 90.0f, 165.0f, 90.0f, 1.2f,
                    bright, true);
    amber_draw_line(layer, layout, 165.0f, 90.0f, 157.0f, 100.0f, 1.2f,
                    bright, true);
    amber_draw_line(layer, layout, 157.0f, 100.0f, 166.0f, 100.0f, 1.2f,
                    bright, true);
    amber_draw_line(layer, layout, 166.0f, 100.0f, 153.0f, 114.0f, 1.2f,
                    bright, true);
}

static void draw_advance_gauge(lv_layer_t *layer, const ui_layout_t *layout,
                               float advance) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();
    const float progress = amber_progress(advance, ADVANCE_MIN, ADVANCE_MAX);

    // Une piste fine et sept repères de 10° donnent l'ordre de grandeur sans
    // transformer le haut de l'écran en seconde jauge.
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, ADVANCE_RADIUS, 4.5f,
                   ADVANCE_START, ADVANCE_START + ADVANCE_SWEEP, dim, false);
    if (progress > 0.0f) {
        amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, ADVANCE_RADIUS,
                       7.0f, ADVANCE_START,
                       (float)lroundf(ADVANCE_START +
                                      ADVANCE_SWEEP * progress), bright, false);
    }

    // Sept repères principaux correspondent directement à -10, 0, 10...50°.
    for (int i = 0; i < ADVANCE_TICKS; i++) {
        const float fraction = (float)i / (float)(ADVANCE_TICKS - 1);
        const float angle = ADVANCE_START + ADVANCE_SWEEP * fraction;
        const bool active = fraction <= progress;
        const bool endpoint = i == 0 || i == ADVANCE_TICKS - 1;
        const lv_color_t color = active ? bright : (endpoint ? separator : dim);
        amber_draw_tick(layer, layout, SCREEN_CX, SCREEN_CY, angle,
                        128.0f, 138.0f, endpoint ? 1.6f : TICK_W, color,
                        false);
    }
}

static void canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    ignition_screen_t *screen = lv_obj_get_user_data(canvas);
    if (layer == NULL || screen == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);

    draw_advance_gauge(layer, &layout, screen->advance_value);
    draw_spark_symbol(layer, &layout);
}

static bool update_pair(lv_obj_t *front, lv_obj_t *shadow, char *buffer,
                        size_t buffer_size, const char *format, float value) {
    char next[24];
    snprintf(next, sizeof(next), format, value);
    // Les libellés sont français : la virgule décimale reste déterministe,
    // sans dépendre de la locale globale de l'ESP-IDF.
    for (char *cursor = next; *cursor != '\0'; cursor++) {
        if (*cursor == '.') *cursor = ',';
    }
    if (strcmp(buffer, next) == 0) return false;
    snprintf(buffer, buffer_size, "%s", next);
    lv_label_set_text_static(front, buffer);
    lv_label_set_text_static(shadow, buffer);
    return true;
}

static void scale_metric_value(lv_obj_t *label) {
    if (label == NULL) return;
    lv_obj_update_layout(label);
    lv_obj_set_style_transform_pivot_x(label, lv_obj_get_width(label) / 2, 0);
    lv_obj_set_style_transform_pivot_y(label, lv_obj_get_height(label) / 2, 0);
    lv_obj_set_style_transform_scale_x(label, METRIC_SCALE, 0);
    lv_obj_set_style_transform_scale_y(label, METRIC_SCALE, 0);
}

ignition_screen_t *ignition_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    ignition_screen_t *screen = lv_malloc(sizeof(*screen));
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

    screen->advance_text[0] = '0';
    screen->advance_text[1] = '\0';
    screen->offset_text[0] = '0';
    screen->offset_text[1] = '\0';
    screen->coil_1_text[0] = '0';
    screen->coil_1_text[1] = '\0';
    screen->coil_2_text[0] = '0';
    screen->coil_2_text[1] = '\0';
    screen->coil_total_text[0] = '0';
    screen->coil_total_text[1] = '\0';

    // Le calque d'ombre est créé avant la valeur pour simuler le faux-gras
    // Michroma sans seconde graisse de police.
    screen->advance_shadow = amber_ui_label_create(
        screen->root, font_xl, ui_theme_amber_dim(), screen->advance_text, 0.0f);
    screen->advance = amber_ui_label_create(
        screen->root, font_xl, ui_theme_amber_bright(), screen->advance_text,
        0.0f);
    screen->advance_caption = amber_ui_label_create(
        screen->root, font_caption, ui_theme_amber_dim(), "AVANCE", 0.0f);

    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t separator = ui_theme_amber_separator();
    const lv_color_t dim = ui_theme_amber_dim();
    screen->metric_shadow[0] = amber_ui_label_create(
        screen->root, font_value, separator, screen->offset_text, 0.0f);
    screen->metric_shadow[1] = amber_ui_label_create(
        screen->root, font_value, separator, screen->coil_1_text, 0.0f);
    screen->metric_shadow[2] = amber_ui_label_create(
        screen->root, font_value, separator, screen->coil_2_text, 0.0f);
    screen->metric_shadow[3] = amber_ui_label_create(
        screen->root, font_value, separator, screen->coil_total_text, 0.0f);
    screen->metric_value[0] = amber_ui_label_create(
        screen->root, font_value, bright, screen->offset_text, 0.0f);
    screen->metric_value[1] = amber_ui_label_create(
        screen->root, font_value, bright, screen->coil_1_text, 0.0f);
    screen->metric_value[2] = amber_ui_label_create(
        screen->root, font_value, bright, screen->coil_2_text, 0.0f);
    screen->metric_value[3] = amber_ui_label_create(
        screen->root, font_value, bright, screen->coil_total_text, 0.0f);
    screen->metric_name[0] = amber_ui_label_create(
        screen->root, font_caption, dim, "CORR.", 0.0f);
    screen->metric_name[1] = amber_ui_label_create(
        screen->root, font_caption, dim, "BOB.1", 0.0f);
    screen->metric_name[2] = amber_ui_label_create(
        screen->root, font_caption, dim, "BOB.2", 0.0f);
    screen->metric_name[3] = amber_ui_label_create(
        screen->root, font_caption, dim, "DUR.", 0.0f);

    if (screen->advance_shadow == NULL || screen->advance == NULL ||
        screen->advance_caption == NULL || screen->metric_shadow[0] == NULL ||
        screen->metric_shadow[1] == NULL || screen->metric_shadow[2] == NULL ||
        screen->metric_shadow[3] == NULL || screen->metric_value[0] == NULL ||
        screen->metric_value[1] == NULL || screen->metric_value[2] == NULL ||
        screen->metric_value[3] == NULL || screen->metric_name[0] == NULL ||
        screen->metric_name[1] == NULL || screen->metric_name[2] == NULL ||
        screen->metric_name[3] == NULL) goto fail;

    lv_obj_add_flag(screen->advance_shadow, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 4; i++) {
        lv_obj_add_flag(screen->metric_shadow[i], LV_OBJ_FLAG_HIDDEN);
    }

    amber_ui_place_centered(
        screen->advance_shadow, screen->root, SCREEN_CX, 143.0f, 0.0f,
        amber_ui_bold_spread(BOLD_SPREAD_PX));
    amber_ui_place_centered(
        screen->advance, screen->root, SCREEN_CX, 143.0f, 0.0f,
        -amber_ui_bold_spread(BOLD_SPREAD_PX));
    amber_ui_place_centered(screen->advance_caption, screen->root, SCREEN_CX,
                             181.0f, 0.0f, 0.0f);

    for (int i = 0; i < 4; i++) {
        amber_ui_place_centered(
            screen->metric_shadow[i], screen->root, k_metric_x[i],
            k_metric_value_y[i], 0.0f, amber_ui_bold_spread(BOLD_SPREAD_PX));
        amber_ui_place_centered(
            screen->metric_value[i], screen->root, k_metric_x[i],
            k_metric_value_y[i], 0.0f, -amber_ui_bold_spread(BOLD_SPREAD_PX));
        amber_ui_place_centered(screen->metric_name[i], screen->root,
                                k_metric_x[i], k_metric_name_y[i], 0.0f,
                                0.0f);
        scale_metric_value(screen->metric_shadow[i]);
        scale_metric_value(screen->metric_value[i]);
    }

    return screen;

fail:
    ignition_screen_destroy(screen);
    return NULL;
}

void ignition_screen_update(ignition_screen_t *screen, const ecu_data_t *data) {
    if (screen == NULL || data == NULL || screen->canvas == NULL) return;

    const int32_t advance_bucket = isfinite(data->ignition_advance)
        ? (int32_t)lroundf(amber_progress(data->ignition_advance,
                                           ADVANCE_MIN, ADVANCE_MAX) * 3600.0f)
        : 0;
    const bool advance_changed = !screen->has_snapshot ||
                                  advance_bucket != screen->advance_bucket;
    const bool advance_text_changed = update_pair(
        screen->advance, screen->advance_shadow, screen->advance_text,
        sizeof(screen->advance_text), "%.0f°", data->ignition_advance);
    const bool metric_changed[4] = {
        update_pair(screen->metric_value[0], screen->metric_shadow[0],
                    screen->offset_text, sizeof(screen->offset_text), "%.1f°",
                    data->ignition_advance_offset),
        update_pair(screen->metric_value[1], screen->metric_shadow[1],
                    screen->coil_1_text, sizeof(screen->coil_1_text), "%.2f ms",
                    data->coil_1_charge_time),
        update_pair(screen->metric_value[2], screen->metric_shadow[2],
                    screen->coil_2_text, sizeof(screen->coil_2_text), "%.2f ms",
                    data->coil_2_charge_time),
        update_pair(screen->metric_value[3], screen->metric_shadow[3],
                    screen->coil_total_text, sizeof(screen->coil_total_text),
                    "%.2f ms", data->coil_time_microseconds / 1000.0f),
    };

    if (advance_text_changed) {
        amber_ui_place_centered(
            screen->advance_shadow, screen->root, SCREEN_CX, 143.0f, 0.0f,
            amber_ui_bold_spread(BOLD_SPREAD_PX));
        amber_ui_place_centered(
            screen->advance, screen->root, SCREEN_CX, 143.0f, 0.0f,
            -amber_ui_bold_spread(BOLD_SPREAD_PX));
    }
    for (int i = 0; i < 4; i++) {
        if (!metric_changed[i]) continue;
        amber_ui_place_centered(
            screen->metric_shadow[i], screen->root, k_metric_x[i],
            k_metric_value_y[i], 0.0f, amber_ui_bold_spread(BOLD_SPREAD_PX));
        amber_ui_place_centered(
            screen->metric_value[i], screen->root, k_metric_x[i],
            k_metric_value_y[i], 0.0f, -amber_ui_bold_spread(BOLD_SPREAD_PX));
        scale_metric_value(screen->metric_shadow[i]);
        scale_metric_value(screen->metric_value[i]);
    }
    if (advance_changed) {
        screen->advance_value = data->ignition_advance;
        screen->advance_bucket = advance_bucket;
        lv_obj_invalidate(screen->canvas);
    }
    screen->has_snapshot = true;
}

void ignition_screen_destroy(ignition_screen_t *screen) {
    if (screen == NULL) return;
    if (screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
