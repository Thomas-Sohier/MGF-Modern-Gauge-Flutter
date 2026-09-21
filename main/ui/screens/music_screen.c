#include "ui/screens/music_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"
#include "ui/widgets/amber_ui.h"
#include "ui/widgets/amber_value.h"

#include <math.h>

// Repère de conception partagé avec style_amber.c (320 x 320).
#define MUSIC_CX              160.0f
#define MUSIC_CY              160.0f
#define RING_RADIUS           151.0f
#define RING_WIDTH              8.0f
#define DISC_CY               101.0f
#define DISC_RADIUS            57.0f
#define DISC_INNER_RADIUS      47.0f
#define DISC_CORE_RADIUS       35.0f
#define TITLE_Y               169.0f
#define ARTIST_Y              187.0f
#define CONTROL_Y             235.0f
#define TIME_Y                235.0f
#define CONTROL_LEFT          111.0f
#define CONTROL_RIGHT         209.0f
#define PLAY_RADIUS            27.0f
#define SIDE_CONTROL_RADIUS    15.0f

#define DEMO_PROGRESS           0.62f
#define DEMO_TITLE              "MIDNIGHT DRIVE"
#define DEMO_ARTIST             "MGF / SYNTHWAVE"
#define DEMO_POSITION           "02:22"
#define DEMO_DURATION           "03:48"

struct music_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;
    amber_value_widget_t *title;
    lv_obj_t *artist;
    lv_obj_t *position;
    lv_obj_t *duration;
    float progress;
};

static ui_layout_t layout_of(const lv_area_t *area) {
    ui_layout_t layout = ui_layout_fit(lv_area_get_width(area),
                                        lv_area_get_height(area));
    layout.ox += area->x1;
    layout.oy += area->y1;
    return layout;
}

static void draw_line(lv_layer_t *layer, float x1, float y1, float x2, float y2,
                      float width, lv_color_t color) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.width = LV_MAX(1, (int32_t)lroundf(width));
    dsc.round_start = 0;
    dsc.round_end = 0;
    dsc.p1.x = x1;
    dsc.p1.y = y1;
    dsc.p2.x = x2;
    dsc.p2.y = y2;
    lv_draw_line(layer, &dsc);
}

static void draw_arc(lv_layer_t *layer, int32_t cx, int32_t cy, float radius,
                     float width, float start, float end, lv_color_t color,
                     bool rounded) {
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.center.x = cx;
    dsc.center.y = cy;
    dsc.radius = (int32_t)lroundf(radius);
    dsc.width = LV_MAX(1, (int32_t)lroundf(width));
    dsc.start_angle = (uint16_t)lroundf(start);
    dsc.end_angle = (uint16_t)lroundf(end);
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.rounded = rounded;
    lv_draw_arc(layer, &dsc);
}

static void draw_full_arc(lv_layer_t *layer, int32_t cx, int32_t cy,
                          float radius, float width, lv_color_t color) {
    draw_arc(layer, cx, cy, radius, width, 0.0f, 360.0f, color, false);
}

// LVGL accepte des angles dans [0, 360]. Cette petite découpe conserve le
// départ à 12 h lorsque l'arc de progression franchit 3 h.
static void draw_wrapped_arc(lv_layer_t *layer, int32_t cx, int32_t cy,
                             float radius, float width, float start,
                             float sweep, lv_color_t color) {
    if (sweep <= 0.0f) return;
    if (sweep >= 360.0f) {
        draw_full_arc(layer, cx, cy, radius, width, color);
        return;
    }

    const float first_end = 360.0f - start;
    if (sweep <= first_end) {
        draw_arc(layer, cx, cy, radius, width, start, start + sweep, color,
                 false);
        return;
    }

    draw_arc(layer, cx, cy, radius, width, start, 360.0f, color, false);
    draw_arc(layer, cx, cy, radius, width, 0.0f, sweep - first_end, color,
             false);
}

static void draw_filled_circle(lv_layer_t *layer, int32_t cx, int32_t cy,
                               float radius, lv_color_t color) {
    // Un arc plein évite une dépendance à une primitive de remplissage et
    // reste disponible dans les configurations LVGL vectorielles minimales.
    draw_full_arc(layer, cx, cy, radius, radius * 2.0f, color);
}

static void draw_disc(lv_layer_t *layer, const ui_layout_t *layout) {
    const int32_t cx = (int32_t)lroundf(ui_layout_x(layout, MUSIC_CX));
    const int32_t cy = (int32_t)lroundf(ui_layout_y(layout, DISC_CY));
    const float scale = layout->scale;

    draw_filled_circle(layer, cx, cy, DISC_RADIUS * scale,
                       ui_theme_amber_bg());
    draw_full_arc(layer, cx, cy, DISC_RADIUS * scale, 1.4f * scale,
                  ui_theme_amber_separator());
    draw_full_arc(layer, cx, cy, DISC_INNER_RADIUS * scale, 1.0f * scale,
                  ui_theme_amber_dim());
    draw_full_arc(layer, cx, cy, DISC_CORE_RADIUS * scale, 0.8f * scale,
                  ui_theme_amber_separator());

    // Note double vectorielle, utilisée comme pochette de démonstration.
    const float x1 = ui_layout_x(layout, 164.0f);
    const float x2 = ui_layout_x(layout, 178.0f);
    const float y_top = ui_layout_y(layout, 82.0f);
    const float y_bar = ui_layout_y(layout, 79.0f);
    const float y_bottom = ui_layout_y(layout, 106.0f);
    const float note_width = 2.4f * scale;
    draw_line(layer, x1, y_top, x1, y_bottom, note_width,
              ui_theme_amber_bright());
    draw_line(layer, x1, y_top, x2, y_bar, note_width,
              ui_theme_amber_bright());
    draw_line(layer, x2, y_bar, x2, y_bottom - 2.0f * scale, note_width,
              ui_theme_amber_bright());
    draw_filled_circle(layer, (int32_t)lroundf(ui_layout_x(layout, 161.0f)),
                       (int32_t)lroundf(ui_layout_y(layout, 108.0f)),
                       5.0f * scale, ui_theme_amber_bright());
    draw_filled_circle(layer, (int32_t)lroundf(ui_layout_x(layout, 175.0f)),
                       (int32_t)lroundf(ui_layout_y(layout, 105.0f)),
                       5.0f * scale, ui_theme_amber_bright());
}

static void draw_chevron(lv_layer_t *layer, const ui_layout_t *layout,
                         float x, float y, bool right) {
    const float direction = right ? 1.0f : -1.0f;
    const float w = 7.0f;
    const float h = 8.0f;
    const float x_tip = x + direction * w;
    const float x_back = x - direction * w;
    draw_line(layer, ui_layout_x(layout, x_back), ui_layout_y(layout, y - h),
              ui_layout_x(layout, x_tip), ui_layout_y(layout, y),
              2.2f * layout->scale, ui_theme_amber_bright());
    draw_line(layer, ui_layout_x(layout, x_tip), ui_layout_y(layout, y),
              ui_layout_x(layout, x_back), ui_layout_y(layout, y + h),
              2.2f * layout->scale, ui_theme_amber_bright());
}

static void draw_controls(lv_layer_t *layer, const ui_layout_t *layout,
                          bool playing) {
    const int32_t cx = (int32_t)lroundf(ui_layout_x(layout, MUSIC_CX));
    const int32_t cy = (int32_t)lroundf(ui_layout_y(layout, CONTROL_Y));
    const float scale = layout->scale;

    // Boutons latéraux vectoriels : cercle fin, puis précédent / suivant.
    draw_full_arc(layer, (int32_t)lroundf(ui_layout_x(layout, CONTROL_LEFT)), cy,
                  SIDE_CONTROL_RADIUS * scale, 1.0f * scale,
                  ui_theme_amber_separator());
    draw_full_arc(layer, (int32_t)lroundf(ui_layout_x(layout, CONTROL_RIGHT)), cy,
                  SIDE_CONTROL_RADIUS * scale, 1.0f * scale,
                  ui_theme_amber_separator());
    draw_chevron(layer, layout, CONTROL_LEFT, CONTROL_Y, false);
    draw_chevron(layer, layout, CONTROL_RIGHT, CONTROL_Y, true);
    draw_line(layer, ui_layout_x(layout, CONTROL_LEFT - 10.0f),
              ui_layout_y(layout, CONTROL_Y - 8.0f),
              ui_layout_x(layout, CONTROL_LEFT - 10.0f),
              ui_layout_y(layout, CONTROL_Y + 8.0f),
              2.2f * scale, ui_theme_amber_bright());
    draw_line(layer, ui_layout_x(layout, CONTROL_RIGHT + 10.0f),
              ui_layout_y(layout, CONTROL_Y - 8.0f),
              ui_layout_x(layout, CONTROL_RIGHT + 10.0f),
              ui_layout_y(layout, CONTROL_Y + 8.0f),
              2.2f * scale, ui_theme_amber_bright());

    // Bouton principal plein ambre, avec symbole pause/play brun-noir.
    draw_filled_circle(layer, cx, cy, PLAY_RADIUS * scale,
                       ui_theme_amber_bright());
    if (playing) {
        draw_line(layer, ui_layout_x(layout, 155.0f),
                  ui_layout_y(layout, CONTROL_Y - 8.0f),
                  ui_layout_x(layout, 155.0f),
                  ui_layout_y(layout, CONTROL_Y + 8.0f),
                  3.0f * scale, ui_theme_amber_bg());
        draw_line(layer, ui_layout_x(layout, 165.0f),
                  ui_layout_y(layout, CONTROL_Y - 8.0f),
                  ui_layout_x(layout, 165.0f),
                  ui_layout_y(layout, CONTROL_Y + 8.0f),
                  3.0f * scale, ui_theme_amber_bg());
    } else {
        draw_line(layer, ui_layout_x(layout, 156.0f),
                  ui_layout_y(layout, CONTROL_Y - 10.0f),
                  ui_layout_x(layout, 156.0f),
                  ui_layout_y(layout, CONTROL_Y + 10.0f),
                  3.0f * scale, ui_theme_amber_bg());
        draw_line(layer, ui_layout_x(layout, 156.0f),
                  ui_layout_y(layout, CONTROL_Y - 10.0f),
                  ui_layout_x(layout, 170.0f),
                  ui_layout_y(layout, CONTROL_Y),
                  3.0f * scale, ui_theme_amber_bg());
        draw_line(layer, ui_layout_x(layout, 170.0f),
                  ui_layout_y(layout, CONTROL_Y),
                  ui_layout_x(layout, 156.0f),
                  ui_layout_y(layout, CONTROL_Y + 10.0f),
                  3.0f * scale, ui_theme_amber_bg());
    }
}

static void canvas_draw_cb(lv_event_t *event) {
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    music_screen_t *scr = lv_obj_get_user_data(canvas);
    if (layer == NULL || scr == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = layout_of(&area);
    const int32_t cx = (int32_t)lroundf(ui_layout_x(&layout, MUSIC_CX));
    const int32_t cy = (int32_t)lroundf(ui_layout_y(&layout, MUSIC_CY));
    const float scale = layout.scale;

    draw_full_arc(layer, cx, cy, RING_RADIUS * scale, RING_WIDTH * scale,
                  ui_theme_amber_dim());
    draw_wrapped_arc(layer, cx, cy, RING_RADIUS * scale, RING_WIDTH * scale,
                     270.0f, 360.0f * scr->progress,
                     ui_theme_amber_bright());
    draw_disc(layer, &layout);
    draw_controls(layer, &layout, true);
}

music_screen_t *music_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    music_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));
    scr->progress = DEMO_PROGRESS;

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
    lv_obj_clear_flag(scr->root, LV_OBJ_FLAG_SCROLLABLE);

    scr->canvas = amber_ui_canvas_create(scr->root, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    scr->title = amber_value_widget_create(
        scr->root, amber_ui_font_value(), MUSIC_CX,
        TITLE_Y, 1, DEMO_TITLE);
    if (scr->title == NULL) goto fail;

    scr->artist = amber_ui_label_create(
        scr->root, ui_font_or(ui_font_m, &lv_font_montserrat_20),
        ui_theme_amber_dim(), DEMO_ARTIST, 0.0f);
    amber_ui_place_centered(scr->artist, scr->root, MUSIC_CX, ARTIST_Y, 0.0f,
                            0.0f);
    if (scr->artist == NULL) goto fail;

    scr->position = amber_ui_label_create(
        scr->root, ui_font_or(ui_font_m, &lv_font_montserrat_20),
        ui_theme_amber_dim(), DEMO_POSITION, 0.0f);
    amber_ui_place_centered(scr->position, scr->root, 65.0f, TIME_Y, 0.0f,
                            0.0f);
    if (scr->position == NULL) goto fail;

    scr->duration = amber_ui_label_create(
        scr->root, ui_font_or(ui_font_m, &lv_font_montserrat_20),
        ui_theme_amber_dim(), DEMO_DURATION, 0.0f);
    amber_ui_place_centered(scr->duration, scr->root, 255.0f, TIME_Y, 0.0f,
                            0.0f);
    if (scr->duration == NULL) goto fail;

    return scr;

fail:
    music_screen_destroy(scr);
    return NULL;
}

void music_screen_update(music_screen_t *scr, const ecu_data_t *d) {
    if (scr == NULL || d == NULL || scr->canvas == NULL) return;

    // Le morceau de démonstration est volontairement stable : cette fonction
    // ne touche qu'au rendu et ne modifie jamais la hiérarchie LVGL.
    scr->progress = DEMO_PROGRESS;
    lv_obj_invalidate(scr->canvas);
}

void music_screen_destroy(music_screen_t *scr) {
    if (scr == NULL) return;
    amber_value_widget_destroy(scr->title);
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
