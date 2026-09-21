#include "ui/screens/ignition_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"

#include <math.h>
#include <stdio.h>

// Toutes les coordonnées sont exprimées dans le repère ambre partagé 320 x
// 320. Le contenu est ensuite inscrit dans le plus grand carré disponible.
#define SCREEN_CX 160.0f
#define SCREEN_CY 160.0f
#define OUTER_R  156.0f

#define ADVANCE_MIN       (-10.0f)
#define ADVANCE_MAX       50.0f
#define ADVANCE_START     210.0f
#define ADVANCE_SWEEP     120.0f
#define ADVANCE_RADIUS    140.0f
#define ADVANCE_TICKS     25

#define GRID_Y             224.0f
#define LINE_W             0.8f
#define TICK_W             1.2f
#define BOLD_SPREAD_PX     1

// Zone sûre du disque : les deux colonnes restent dans le cercle intérieur,
// même à la ligne basse. Les libellés courts laissent une vraie gouttière au
// séparateur central ; aucune largeur de valeur ne peut donc créer de collision.
static const float k_metric_x[4] = {100.0f, 220.0f, 110.0f, 210.0f};
static const float k_metric_value_y[4] = {225.0f, 225.0f, 267.0f, 267.0f};
static const float k_metric_name_y[4] = {246.0f, 246.0f, 288.0f, 288.0f};

struct ignition_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;

    lv_obj_t *advance_shadow;
    lv_obj_t *advance;
    lv_obj_t *advance_caption;
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
};


static void draw_spark_symbol(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    // Éclair central : deux épaisseurs ambre donnent un relief net sans
    // introduire de bitmap ni d'icône externe.
    amber_draw_line(layer, layout, 154.0f, 91.0f, 165.0f, 91.0f, 2.6f, dim,
                    true);
    amber_draw_line(layer, layout, 165.0f, 91.0f, 157.0f, 101.0f, 2.6f, dim,
                    true);
    amber_draw_line(layer, layout, 157.0f, 101.0f, 166.0f, 101.0f, 2.6f, dim,
                    true);
    amber_draw_line(layer, layout, 166.0f, 101.0f, 153.0f, 116.0f, 2.6f, dim,
                    true);

    amber_draw_line(layer, layout, 154.0f, 89.0f, 165.0f, 89.0f, 1.2f, bright,
                    true);
    amber_draw_line(layer, layout, 165.0f, 89.0f, 157.0f, 99.0f, 1.2f, bright,
                    true);
    amber_draw_line(layer, layout, 157.0f, 99.0f, 166.0f, 99.0f, 1.2f, bright,
                    true);
    amber_draw_line(layer, layout, 166.0f, 99.0f, 153.0f, 114.0f, 1.2f, bright,
                    true);

    // Trois rayons courts évoquent l'étincelle et équilibrent le motif dans le
    // vide entre l'arc de mesure et la valeur principale.
    amber_draw_line(layer, layout, 143.0f, 98.0f, 137.0f, 95.0f, 0.9f, dim,
                    true);
    amber_draw_line(layer, layout, 174.0f, 98.0f, 180.0f, 95.0f, 0.9f, dim,
                    true);
    amber_draw_line(layer, layout, 160.0f, 78.0f, 160.0f, 71.0f, 0.9f, dim,
                    true);
}

static void draw_advance_gauge(lv_layer_t *layer, const ui_layout_t *layout,
                               float advance) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();
    const float progress = amber_progress(advance, ADVANCE_MIN, ADVANCE_MAX);

    // Anneau extérieur discret, puis couronne segmentée : le cadran reste
    // lisible même quand la valeur est proche de zéro.
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, OUTER_R, 0.8f,
                   0.0f, 360.0f, separator, false);
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, OUTER_R - 4.0f, 0.55f,
                   0.0f, 360.0f, dim, false);

    const float segment_gap = 2.4f;
    const float segment_step = 360.0f / 32.0f;
    for (int i = 0; i < 32; i++) {
        const float start = (float)i * segment_step + segment_gap * 0.5f;
        amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, OUTER_R - 8.0f,
                       1.4f, (float)lroundf(start),
                       (float)lroundf(start + segment_step - segment_gap),
                       separator, false);
    }

    // Piste principale d'avance, avec extrémités droites comme le cadran RPM.
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, ADVANCE_RADIUS, 7.0f,
                   ADVANCE_START, ADVANCE_START + ADVANCE_SWEEP, dim, false);
    if (progress > 0.0f) {
        amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, ADVANCE_RADIUS,
                       7.0f, ADVANCE_START,
                       (float)lroundf(ADVANCE_START +
                                      ADVANCE_SWEEP * progress), bright, false);
    }

    // Graduations principales et secondaires alignées sur la plage -10..50°.
    for (int i = 0; i < ADVANCE_TICKS; i++) {
        const float fraction = (float)i / (float)(ADVANCE_TICKS - 1);
        const float angle = ADVANCE_START + ADVANCE_SWEEP * fraction;
        const bool major = (i % 4) == 0 || i == ADVANCE_TICKS - 1;
        const bool active = fraction <= progress;
        amber_draw_tick(layer, layout, SCREEN_CX, SCREEN_CY, angle,
                        major ? 127.0f : 130.0f,
                        major ? 136.0f : 134.0f, major ? 1.5f : TICK_W,
                        active ? bright : dim, false);
    }

    // Repères d'extrémité : ils donnent une référence physique à la plage
    // sans multiplier les textes sur le cadran rond.
    amber_draw_tick(layer, layout, SCREEN_CX, SCREEN_CY, ADVANCE_START,
                    121.0f, 137.0f, 1.8f, separator, false);
    amber_draw_tick(layer, layout, SCREEN_CX, SCREEN_CY,
                    ADVANCE_START + ADVANCE_SWEEP, 121.0f, 137.0f, 1.8f,
                    separator, false);
}

static void draw_panel_lines(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t separator = ui_theme_amber_separator();

    // Séparateurs ouverts : aucun trait ne vient fermer artificiellement le
    // cercle ni couper les valeurs centrales.
    amber_draw_line(layer, layout, 44.0f, GRID_Y, 127.0f, GRID_Y, LINE_W,
                    separator, false);
    amber_draw_line(layer, layout, 193.0f, GRID_Y, 276.0f, GRID_Y, LINE_W,
                    separator, false);
    // Aucun trait vertical dans la zone des métriques : la gouttière centrale
    // reste vide, y compris lorsque les valeurs affichent leurs unités.

    // Petits repères latéraux, inspirés des cellules relevées du cadran RPM.
    amber_draw_line(layer, layout, 47.0f, GRID_Y + 5.0f, 47.0f, GRID_Y + 12.0f,
                    LINE_W, separator, false);
    amber_draw_line(layer, layout, 273.0f, GRID_Y + 5.0f, 273.0f, GRID_Y + 12.0f,
                    LINE_W, separator, false);
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
    draw_panel_lines(layer, &layout);
}

static void place_centered(lv_obj_t *object, lv_obj_t *parent, float x,
                           float y, float x_offset) {
    lv_obj_update_layout(object);
    const ui_layout_t layout = ui_layout_fit(lv_obj_get_width(parent),
                                             lv_obj_get_height(parent));
    const float offset = x_offset * layout.scale * UI_REFERENCE_SIZE /
                         UI_DISPLAY_SIZE_PX;
    lv_obj_set_pos(object,
                   lroundf(ui_layout_x(&layout, x)) -
                       lv_obj_get_width(object) / 2 + lroundf(offset),
                   lroundf(ui_layout_y(&layout, y)) -
                       lv_obj_get_height(object) / 2);
}

static lv_obj_t *label_create(lv_obj_t *parent, const lv_font_t *font,
                              lv_color_t color, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    if (label == NULL) return NULL;

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text_static(label, text != NULL ? text : "");
    return label;
}

static void update_label(lv_obj_t *label, char *buffer, size_t buffer_size,
                         const char *format, float value) {
    if (label == NULL || buffer == NULL || buffer_size == 0) return;
    snprintf(buffer, buffer_size, format, value);
    // Les libellés sont français : la virgule décimale reste déterministe,
    // sans dépendre de la locale globale de l'ESP-IDF.
    for (char *cursor = buffer; *cursor != '\0'; cursor++) {
        if (*cursor == '.') *cursor = ',';
    }
    lv_label_set_text_static(label, buffer);
}

ignition_screen_t *ignition_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    ignition_screen_t *screen = lv_malloc(sizeof(*screen));
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
    screen->advance_shadow = label_create(screen->root, font_xl,
                                          ui_theme_amber_dim(),
                                          screen->advance_text);
    screen->advance = label_create(screen->root, font_xl,
                                   ui_theme_amber_bright(),
                                   screen->advance_text);
    screen->advance_caption = label_create(screen->root, font_caption,
                                           ui_theme_amber_dim(),
                                           "AVANCE D'ALLUMAGE");
    screen->metric_value[0] = label_create(screen->root, font_value,
                                           ui_theme_amber_bright(),
                                           screen->offset_text);
    screen->metric_value[1] = label_create(screen->root, font_value,
                                           ui_theme_amber_bright(),
                                           screen->coil_1_text);
    screen->metric_value[2] = label_create(screen->root, font_value,
                                           ui_theme_amber_bright(),
                                           screen->coil_2_text);
    screen->metric_value[3] = label_create(screen->root, font_value,
                                           ui_theme_amber_bright(),
                                           screen->coil_total_text);
    screen->metric_name[0] = label_create(screen->root, font_caption,
                                          ui_theme_amber_dim(), "CORR.");
    screen->metric_name[1] = label_create(screen->root, font_caption,
                                          ui_theme_amber_dim(), "BOB. 1");
    screen->metric_name[2] = label_create(screen->root, font_caption,
                                          ui_theme_amber_dim(), "BOB. 2");
    screen->metric_name[3] = label_create(screen->root, font_caption,
                                          ui_theme_amber_dim(), "DUREE");

    if (screen->advance_shadow == NULL || screen->advance == NULL ||
        screen->advance_caption == NULL || screen->metric_value[0] == NULL ||
        screen->metric_value[1] == NULL || screen->metric_value[2] == NULL ||
        screen->metric_value[3] == NULL || screen->metric_name[0] == NULL ||
        screen->metric_name[1] == NULL || screen->metric_name[2] == NULL ||
        screen->metric_name[3] == NULL) goto fail;

    place_centered(screen->advance_shadow, screen->root, SCREEN_CX, 143.0f,
                   BOLD_SPREAD_PX);
    place_centered(screen->advance, screen->root, SCREEN_CX, 143.0f,
                   -BOLD_SPREAD_PX);
    place_centered(screen->advance_caption, screen->root, SCREEN_CX, 181.0f,
                   0.0f);

    for (int i = 0; i < 4; i++) {
        place_centered(screen->metric_value[i], screen->root, k_metric_x[i],
                       k_metric_value_y[i], 0.0f);
        place_centered(screen->metric_name[i], screen->root, k_metric_x[i],
                       k_metric_name_y[i], 0.0f);
    }

    return screen;

fail:
    ignition_screen_destroy(screen);
    return NULL;
}

void ignition_screen_update(ignition_screen_t *screen, const ecu_data_t *data) {
    if (screen == NULL || data == NULL || screen->canvas == NULL) return;

    screen->advance_value = data->ignition_advance;
    update_label(screen->advance, screen->advance_text,
                 sizeof(screen->advance_text), "%.0f°", data->ignition_advance);
    update_label(screen->advance_shadow, screen->advance_text,
                 sizeof(screen->advance_text), "%.0f°", data->ignition_advance);
    update_label(screen->metric_value[0], screen->offset_text,
                 sizeof(screen->offset_text), "%.1f°",
                 data->ignition_advance_offset);
    update_label(screen->metric_value[1], screen->coil_1_text,
                 sizeof(screen->coil_1_text), "%.2f ms",
                 data->coil_1_charge_time);
    update_label(screen->metric_value[2], screen->coil_2_text,
                 sizeof(screen->coil_2_text), "%.2f ms",
                 data->coil_2_charge_time);
    update_label(screen->metric_value[3], screen->coil_total_text,
                 sizeof(screen->coil_total_text), "%.2f ms",
                 data->coil_time_microseconds / 1000.0f);

    // Les largeurs changent avec les chiffres : le repositionnement est purement
    // géométrique et n'alloue rien, tandis que le canvas redessine l'arc.
    place_centered(screen->advance_shadow, screen->root, SCREEN_CX, 143.0f,
                   BOLD_SPREAD_PX);
    place_centered(screen->advance, screen->root, SCREEN_CX, 143.0f,
                   -BOLD_SPREAD_PX);
    for (int i = 0; i < 4; i++) {
        place_centered(screen->metric_value[i], screen->root, k_metric_x[i],
                       k_metric_value_y[i], 0.0f);
    }
    lv_obj_invalidate(screen->canvas);
}

void ignition_screen_destroy(ignition_screen_t *screen) {
    if (screen == NULL) return;
    if (screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
