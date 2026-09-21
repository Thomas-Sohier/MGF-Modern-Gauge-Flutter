#include "ui/screens/navigation_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_value.h"

#include <math.h>

// Repère logique commun aux écrans ambre : 320 x 320, centré et mis à
// l'échelle uniformément dans le plus grand carré disponible.
#define NAV_CX 160.0f
#define NAV_ROUTE_CY 105.0f
#define NAV_ROUTE_R 62.0f
#define NAV_SEPARATOR_Y 258.0f

#define NAV_LINE_W 1.0f
#define NAV_ROUTE_ROAD_W 11.0f
#define NAV_ROUTE_CORE_W 3.0f
#define NAV_ARROW_W 4.0f

struct navigation_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;
    lv_obj_t *header;
    lv_obj_t *instruction;
    lv_obj_t *street;
    lv_obj_t *arrival;
    amber_value_widget_t *distance;
    amber_value_widget_t *distance_unit;
};

static void draw_route(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t road = ui_theme_amber_separator();
    const lv_color_t core = ui_theme_amber_bright();
    const lv_color_t guide = ui_theme_amber_dim();
    ui_layout_t line_layout = *layout;
    line_layout.ox -= 0.5f;
    line_layout.oy -= 0.5f;

    // Deux courbes fines donnent un repère de carte sans remplir la surface.
    amber_draw_arc(layer, layout, NAV_CX, NAV_ROUTE_CY, NAV_ROUTE_R,
                   NAV_LINE_W, 205.0f, 335.0f, guide, false);
    amber_draw_arc(layer, layout, NAV_CX, NAV_ROUTE_CY, NAV_ROUTE_R + 8.0f,
                   NAV_LINE_W, 205.0f, 335.0f, road, false);

    // Route principale : couche large sombre/ambre puis liseré lumineux.
    amber_draw_line(layer, &line_layout, 154.0f, 166.0f, 154.0f, 137.0f,
                    NAV_ROUTE_ROAD_W, road, true);
    amber_draw_line(layer, &line_layout, 154.0f, 137.0f, 164.0f, 119.0f,
                    NAV_ROUTE_ROAD_W, road, true);
    amber_draw_line(layer, &line_layout, 164.0f, 119.0f, 184.0f, 105.0f,
                    NAV_ROUTE_ROAD_W, road, true);
    amber_draw_line(layer, &line_layout, 184.0f, 105.0f, 219.0f, 105.0f,
                    NAV_ROUTE_ROAD_W, road, true);

    amber_draw_line(layer, &line_layout, 154.0f, 166.0f, 154.0f, 137.0f,
                    NAV_ROUTE_CORE_W, core, true);
    amber_draw_line(layer, &line_layout, 154.0f, 137.0f, 164.0f, 119.0f,
                    NAV_ROUTE_CORE_W, core, true);
    amber_draw_line(layer, &line_layout, 164.0f, 119.0f, 184.0f, 105.0f,
                    NAV_ROUTE_CORE_W, core, true);
    amber_draw_line(layer, &line_layout, 184.0f, 105.0f, 219.0f, 105.0f,
                    NAV_ROUTE_CORE_W, core, true);

    // Flèche de manœuvre droite, également entièrement vectorielle.
    amber_draw_line(layer, &line_layout, 219.0f, 105.0f, 207.0f, 94.0f,
                    NAV_ARROW_W, core, true);
    amber_draw_line(layer, &line_layout, 219.0f, 105.0f, 207.0f, 116.0f,
                    NAV_ARROW_W, core, true);

    // Petit embranchement secondaire : il rend la géométrie lisible comme une
    // route, sans introduire de bitmap ou d'icône externe.
    amber_draw_line(layer, &line_layout, 164.0f, 119.0f, 143.0f, 110.0f,
                    NAV_LINE_W, guide, true);
    amber_draw_line(layer, &line_layout, 143.0f, 110.0f, 126.0f, 110.0f,
                    NAV_LINE_W, guide, true);
}

static void canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    ui_layout_t layout = amber_draw_layout(&area);
    layout.ox += area.x1;
    layout.oy += area.y1;
    ui_layout_t line_layout = layout;
    line_layout.ox -= 0.5f;
    line_layout.oy -= 0.5f;

    draw_route(layer, &layout);

    // Séparateur ouvert, comme dans le cadran RPM : les marges évitent une
    // grille fermée et laissent respirer le disque rond.
    amber_draw_line(layer, &line_layout, 38.0f, NAV_SEPARATOR_Y, 104.0f,
                    NAV_SEPARATOR_Y, NAV_LINE_W, ui_theme_amber_separator(), false);
    amber_draw_line(layer, &line_layout, 216.0f, NAV_SEPARATOR_Y, 282.0f,
                    NAV_SEPARATOR_Y, NAV_LINE_W, ui_theme_amber_separator(), false);
}

static lv_obj_t *label_create(lv_obj_t *parent, const lv_font_t *font,
                              lv_color_t color, const char *text) {
    lv_obj_t *label = lv_label_create(parent);
    if (label == NULL) return NULL;

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label, text);
    return label;
}

static void place_centered(lv_obj_t *label, lv_obj_t *parent,
                           float ref_x, float ref_y) {
    lv_obj_update_layout(label);
    const lv_area_t area = {
        0, 0, lv_obj_get_width(parent) - 1, lv_obj_get_height(parent) - 1
    };
    const ui_layout_t layout = amber_draw_layout(&area);
    const int32_t width = lv_obj_get_width(label);
    const int32_t height = lv_obj_get_height(label);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT,
                 lroundf(ui_layout_x(&layout, ref_x)) - width / 2,
                 lroundf(ui_layout_y(&layout, ref_y)) - height / 2);
}

navigation_screen_t *navigation_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    navigation_screen_t *scr = lv_malloc(sizeof(*scr));
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

    const lv_font_t *font_m = ui_font_or(ui_font_m, &lv_font_montserrat_20);
    const lv_font_t *font_xl = ui_font_or(ui_font_xl, &lv_font_montserrat_48);
    const lv_font_t *font_small = ui_font_or(ui_font_l, &lv_font_montserrat_14);

    scr->header = label_create(scr->root, font_small,
                               ui_theme_amber_dim(), "NAVIGATION");
    if (scr->header == NULL) goto fail;
    place_centered(scr->header, scr->root, NAV_CX, 27.0f);

    scr->instruction = label_create(scr->root, font_m,
                                    ui_theme_amber_bright(),
                                    "TOURNEZ A DROITE");
    if (scr->instruction == NULL) goto fail;
    place_centered(scr->instruction, scr->root, NAV_CX, 177.0f);

    scr->distance = amber_value_widget_create(
        scr->root, font_xl, 137.0f, 218.0f, 2, "300");
    if (scr->distance == NULL) goto fail;

    scr->distance_unit = amber_value_widget_create(
        scr->root, font_m, 213.0f, 228.0f, 1, "M");
    if (scr->distance_unit == NULL) goto fail;

    scr->street = label_create(scr->root, font_small,
                                ui_theme_amber_bright(), "RUE DES LILAS");
    if (scr->street == NULL) goto fail;
    place_centered(scr->street, scr->root, NAV_CX, 274.0f);

    scr->arrival = label_create(scr->root, font_small,
                                ui_theme_amber_dim(),
                                "ETA 14:32");
    if (scr->arrival == NULL) goto fail;
    place_centered(scr->arrival, scr->root, NAV_CX, 298.0f);

    return scr;

fail:
    navigation_screen_destroy(scr);
    return NULL;
}

void navigation_screen_update(navigation_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL || scr->canvas == NULL) return;

    // ecu_data_t ne contient pas encore de navigation. Le scénario de démo est
    // donc stable ; l'invalidation garde toutefois le contrat d'écran commun.
    lv_obj_invalidate(scr->canvas);
}

void navigation_screen_destroy(navigation_screen_t *scr) {
    if (scr == NULL) return;

    amber_value_widget_destroy(scr->distance);
    amber_value_widget_destroy(scr->distance_unit);
    if (scr->header != NULL) lv_obj_delete(scr->header);
    if (scr->instruction != NULL) lv_obj_delete(scr->instruction);
    if (scr->street != NULL) lv_obj_delete(scr->street);
    if (scr->arrival != NULL) lv_obj_delete(scr->arrival);
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
