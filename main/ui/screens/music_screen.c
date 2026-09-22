#include "ui/screens/music_screen.h"

#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"
#include "ui/widgets/amber_ui.h"
#include "ui/widgets/amber_value.h"

#include <math.h>

// Repère de conception partagé avec style_amber.c (320 x 320).
#define MUSIC_CX              160.0f
#define ART_CY                 65.0f
#define ART_RADIUS             34.0f
#define TITLE_Y               125.0f
#define ARTIST_Y              163.0f
#define PROGRESS_Y            195.0f
#define TIME_Y                216.0f
#define CONTROL_Y             263.0f
#define CONTROL_LEFT           92.0f
#define CONTROL_RIGHT         228.0f
#define PLAY_RADIUS            30.0f
#define SIDE_CONTROL_RADIUS    22.0f
#define CONTROL_TARGET_SIZE    44.0f
#define PROGRESS_LEFT          54.0f
#define PROGRESS_RIGHT        266.0f
#define MUSIC_TEXT_WIDTH      276.0f

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
    lv_obj_t *previous_button;
    lv_obj_t *play_button;
    lv_obj_t *next_button;
    float progress;
    bool playing;
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

static void draw_filled_circle(lv_layer_t *layer, int32_t cx, int32_t cy,
                               float radius, lv_color_t color) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = (int32_t)lroundf(radius);

    const int32_t r = (int32_t)lroundf(radius);
    const lv_area_t area = {cx - r, cy - r, cx + r, cy + r};
    lv_draw_rect(layer, &dsc, &area);
}

static void draw_art(lv_layer_t *layer, const ui_layout_t *layout) {
    const int32_t cx = (int32_t)lroundf(ui_layout_x(layout, MUSIC_CX));
    const int32_t cy = (int32_t)lroundf(ui_layout_y(layout, ART_CY));
    const float scale = layout->scale;

    // Une seule pastille pleine : l'icône reste lisible sans empiler des
    // anneaux décoratifs qui concurrencent le titre et la progression.
    draw_filled_circle(layer, cx, cy, ART_RADIUS * scale,
                       ui_theme_amber_dim());

    // Pochette de démonstration réduite à une note, entièrement vectorielle.
    const float x1 = ui_layout_x(layout, 153.0f);
    const float x2 = ui_layout_x(layout, 170.0f);
    const float y_top = ui_layout_y(layout, 48.0f);
    const float y_bar = ui_layout_y(layout, 45.0f);
    const float y_bottom = ui_layout_y(layout, 75.0f);
    const float note_width = 2.8f * scale;
    draw_line(layer, x1, y_top, x1, y_bottom, note_width,
              ui_theme_amber_bright());
    draw_line(layer, x1, y_top, x2, y_bar, note_width,
              ui_theme_amber_bright());
    draw_line(layer, x2, y_bar, x2, y_bottom - 2.0f * scale, note_width,
              ui_theme_amber_bright());
    draw_filled_circle(layer, (int32_t)lroundf(ui_layout_x(layout, 149.0f)),
                       (int32_t)lroundf(ui_layout_y(layout, 78.0f)),
                       5.5f * scale, ui_theme_amber_bright());
    draw_filled_circle(layer, (int32_t)lroundf(ui_layout_x(layout, 166.0f)),
                       (int32_t)lroundf(ui_layout_y(layout, 75.0f)),
                       5.5f * scale, ui_theme_amber_bright());
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
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t bg = ui_theme_amber_bg();

    // Les pastilles sont à la fois le repère visuel et la cible tactile :
    // 44 unités logiques donnent 66 px sur le panneau 480 px.
    draw_filled_circle(layer, (int32_t)lroundf(ui_layout_x(layout, CONTROL_LEFT)),
                       cy, SIDE_CONTROL_RADIUS * scale, dim);
    draw_filled_circle(layer, (int32_t)lroundf(ui_layout_x(layout, CONTROL_RIGHT)),
                       cy, SIDE_CONTROL_RADIUS * scale, dim);
    draw_chevron(layer, layout, CONTROL_LEFT, CONTROL_Y, false);
    draw_chevron(layer, layout, CONTROL_RIGHT, CONTROL_Y, true);
    draw_line(layer, ui_layout_x(layout, CONTROL_LEFT - 10.0f),
              ui_layout_y(layout, CONTROL_Y - 9.0f),
              ui_layout_x(layout, CONTROL_LEFT - 10.0f),
              ui_layout_y(layout, CONTROL_Y + 9.0f),
              2.8f * scale, bright);
    draw_line(layer, ui_layout_x(layout, CONTROL_RIGHT + 10.0f),
              ui_layout_y(layout, CONTROL_Y - 9.0f),
              ui_layout_x(layout, CONTROL_RIGHT + 10.0f),
              ui_layout_y(layout, CONTROL_Y + 9.0f),
              2.8f * scale, bright);

    // Bouton principal plus grand, avec symbole pause/play brun-noir.
    draw_filled_circle(layer, cx, cy, PLAY_RADIUS * scale, bright);
    if (playing) {
        draw_line(layer, ui_layout_x(layout, 154.0f),
                  ui_layout_y(layout, CONTROL_Y - 9.0f),
                  ui_layout_x(layout, 154.0f),
                  ui_layout_y(layout, CONTROL_Y + 9.0f),
                  3.6f * scale, bg);
        draw_line(layer, ui_layout_x(layout, 166.0f),
                  ui_layout_y(layout, CONTROL_Y - 9.0f),
                  ui_layout_x(layout, 166.0f),
                  ui_layout_y(layout, CONTROL_Y + 9.0f),
                  3.6f * scale, bg);
    } else {
        draw_line(layer, ui_layout_x(layout, 155.0f),
                  ui_layout_y(layout, CONTROL_Y - 11.0f),
                  ui_layout_x(layout, 155.0f),
                  ui_layout_y(layout, CONTROL_Y + 11.0f),
                  3.6f * scale, bg);
        draw_line(layer, ui_layout_x(layout, 155.0f),
                  ui_layout_y(layout, CONTROL_Y - 11.0f),
                  ui_layout_x(layout, 172.0f),
                  ui_layout_y(layout, CONTROL_Y),
                  3.6f * scale, bg);
        draw_line(layer, ui_layout_x(layout, 172.0f),
                  ui_layout_y(layout, CONTROL_Y),
                  ui_layout_x(layout, 155.0f),
                  ui_layout_y(layout, CONTROL_Y + 11.0f),
                  3.6f * scale, bg);
    }
}

static void canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    music_screen_t *scr = lv_obj_get_user_data(canvas);
    if (layer == NULL || scr == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = layout_of(&area);
    const float scale = layout.scale;
    const float progress = scr->progress < 0.0f ? 0.0f
                           : scr->progress > 1.0f ? 1.0f : scr->progress;

    // Un seul indicateur horizontal : discret au repos, lisible quand il est
    // rempli, sans fermer le masque circulaire par un anneau périphérique.
    draw_line(layer, ui_layout_x(&layout, PROGRESS_LEFT),
              ui_layout_y(&layout, PROGRESS_Y),
              ui_layout_x(&layout, PROGRESS_RIGHT),
              ui_layout_y(&layout, PROGRESS_Y),
              3.0f * scale, ui_theme_amber_dim());
    draw_line(layer, ui_layout_x(&layout, PROGRESS_LEFT),
              ui_layout_y(&layout, PROGRESS_Y),
              ui_layout_x(&layout, PROGRESS_LEFT +
                                  (PROGRESS_RIGHT - PROGRESS_LEFT) * progress),
              ui_layout_y(&layout, PROGRESS_Y),
              5.0f * scale, ui_theme_amber_bright());
    draw_art(layer, &layout);
    draw_controls(layer, &layout, scr->playing);
}

static void music_button_event_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;

    lv_obj_t *button = lv_event_get_target(event);
    music_screen_t *scr = lv_obj_get_user_data(button);
    if (scr == NULL) return;

    if (button == scr->play_button) {
        scr->playing = !scr->playing;
        lv_obj_invalidate(scr->canvas);
    }
    // Previous/next are deliberately consumed here: the demo has one stable
    // track, and a tap must not become a dashboard page-navigation click.
}

static lv_obj_t *create_touch_target(lv_obj_t *parent, music_screen_t *scr,
                                     float x, float y) {
    const ui_layout_t layout = ui_layout_fit(lv_obj_get_width(parent),
                                              lv_obj_get_height(parent));
    const int32_t size = (int32_t)lroundf(CONTROL_TARGET_SIZE * layout.scale);
    lv_obj_t *button = lv_obj_create(parent);
    if (button == NULL) return NULL;

    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, size, size);
    lv_obj_set_pos(button,
                   (int32_t)lroundf(ui_layout_x(&layout, x)) - size / 2,
                   (int32_t)lroundf(ui_layout_y(&layout, y)) - size / 2);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_user_data(button, scr);
    lv_obj_add_event_cb(button, music_button_event_cb, LV_EVENT_CLICKED, NULL);
    return button;
}

music_screen_t *music_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    music_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));
    scr->progress = DEMO_PROGRESS;
    scr->playing = true;

    scr->root = amber_ui_root_create(parent);
    if (scr->root == NULL) goto fail;

    scr->canvas = amber_ui_canvas_create(scr->root, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    // Le titre reste nettement plus grand que les métadonnées, mais 48 px
    // garde la chaîne complète dans le diamètre utile du panneau rond.
    scr->title = amber_value_widget_create(
        scr->root, &lv_font_montserrat_48, MUSIC_CX,
        TITLE_Y, 1, DEMO_TITLE);
    if (scr->title == NULL) goto fail;

    scr->artist = amber_ui_label_create(
        scr->root, amber_ui_font_value(), ui_theme_amber_dim(), DEMO_ARTIST,
        MUSIC_TEXT_WIDTH);
    amber_ui_place_centered(scr->artist, scr->root, MUSIC_CX, ARTIST_Y,
                            MUSIC_TEXT_WIDTH, 0.0f);
    if (scr->artist == NULL) goto fail;

    scr->position = amber_ui_label_create(
        scr->root, amber_ui_font_value(), ui_theme_amber_bright(),
        DEMO_POSITION, 0.0f);
    amber_ui_place_centered(scr->position, scr->root, PROGRESS_LEFT, TIME_Y,
                            0.0f, 0.0f);
    if (scr->position == NULL) goto fail;

    scr->duration = amber_ui_label_create(
        scr->root, amber_ui_font_value(), ui_theme_amber_dim(), DEMO_DURATION,
        0.0f);
    amber_ui_place_centered(scr->duration, scr->root, PROGRESS_RIGHT, TIME_Y,
                            0.0f, 0.0f);
    if (scr->duration == NULL) goto fail;

    // Cibles séparées du canvas : elles restent tactiles même si le dessin est
    // redimensionné, et ne perturbent pas le geste de navigation du dashboard.
    scr->previous_button = create_touch_target(
        scr->root, scr, CONTROL_LEFT, CONTROL_Y);
    if (scr->previous_button == NULL) goto fail;
    scr->play_button = create_touch_target(scr->root, scr, MUSIC_CX, CONTROL_Y);
    if (scr->play_button == NULL) goto fail;
    scr->next_button = create_touch_target(
        scr->root, scr, CONTROL_RIGHT, CONTROL_Y);
    if (scr->next_button == NULL) goto fail;

    return scr;

fail:
    music_screen_destroy(scr);
    return NULL;
}

void music_screen_update(music_screen_t *scr, const ecu_data_t *d) {
    // L'écran est événementiel : l'état ECU n'influence pas le morceau de
    // démonstration. Le bouton lecture reste le seul chemin d'invalidation.
    (void)scr;
    (void)d;
}

void music_screen_destroy(music_screen_t *scr) {
    if (scr == NULL) return;
    amber_value_widget_destroy(scr->title);
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
