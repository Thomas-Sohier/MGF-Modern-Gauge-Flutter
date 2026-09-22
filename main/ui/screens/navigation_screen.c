#include "ui/screens/navigation_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"
#include "ui/widgets/amber_value.h"

// Repère logique commun aux écrans ambre : 320 x 320, centré et mis à
// l'échelle uniformément dans le plus grand carré disponible.
#define NAV_CX 160.0f
#define NAV_ROUTE_CORE_W 5.0f
#define NAV_ROUTE_ROAD_W 16.0f
#define NAV_ARROW_CORE_W 7.0f
#define NAV_ARROW_ROAD_W 20.0f

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
    const lv_color_t road = ui_theme_amber_dim();
    const lv_color_t core = ui_theme_amber_bright();
    ui_layout_t line_layout = *layout;
    line_layout.ox -= 0.5f;
    line_layout.oy -= 0.5f;

    // Une seule trajectoire forte concentre l'information sur la manœuvre :
    // couche large de contraste, puis cœur lumineux. Les deux restent
    // vectoriels et suivent automatiquement l'échelle du cadran rond.
    const float route[][4] = {
        {148.0f, 166.0f, 148.0f, 136.0f},
        {148.0f, 136.0f, 171.0f, 107.0f},
        {171.0f, 107.0f, 226.0f, 107.0f},
    };
    for (unsigned i = 0; i < sizeof(route) / sizeof(route[0]); i++) {
        amber_draw_line(layer, &line_layout, route[i][0], route[i][1],
                        route[i][2], route[i][3], NAV_ROUTE_ROAD_W, road, true);
    }
    for (unsigned i = 0; i < sizeof(route) / sizeof(route[0]); i++) {
        amber_draw_line(layer, &line_layout, route[i][0], route[i][1],
                        route[i][2], route[i][3], NAV_ROUTE_CORE_W, core, true);
    }

    // Pointe agrandie et épaissie : elle reste nette même en vision
    // périphérique, tout en conservant la même hiérarchie ambre.
    amber_draw_line(layer, &line_layout, 226.0f, 107.0f, 207.0f, 88.0f,
                    NAV_ARROW_ROAD_W, road, true);
    amber_draw_line(layer, &line_layout, 226.0f, 107.0f, 207.0f, 126.0f,
                    NAV_ARROW_ROAD_W, road, true);
    amber_draw_line(layer, &line_layout, 226.0f, 107.0f, 207.0f, 88.0f,
                    NAV_ARROW_CORE_W, core, true);
    amber_draw_line(layer, &line_layout, 226.0f, 107.0f, 207.0f, 126.0f,
                    NAV_ARROW_CORE_W, core, true);
}

static void canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);
    draw_route(layer, &layout);
}

navigation_screen_t *navigation_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    navigation_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    lv_obj_set_style_bg_color(parent, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_update_layout(parent);

    scr->root = amber_ui_root_create(parent);
    if (scr->root == NULL) goto fail;
    scr->canvas = amber_ui_canvas_create(scr->root, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    const lv_font_t *font_instruction =
        ui_font_or(ui_font_l, &lv_font_montserrat_20);
    const lv_font_t *font_xl = amber_ui_font_hero();
    const lv_font_t *font_caption = ui_font_or(ui_font_m, &lv_font_montserrat_14);

    scr->header = amber_ui_label_create(scr->root, font_caption,
                                        ui_theme_amber_dim(), "NAVIGATION",
                                        0.0f);
    if (scr->header == NULL) goto fail;
    amber_ui_place_centered(scr->header, scr->root, NAV_CX, 27.0f,
                            0.0f, 0.0f);

    scr->instruction = amber_ui_label_create(
        scr->root, font_instruction, ui_theme_amber_bright(), "A DROITE", 0.0f);
    if (scr->instruction == NULL) goto fail;
    amber_ui_place_centered(scr->instruction, scr->root, NAV_CX, 179.0f,
                            0.0f, 0.0f);

    scr->distance = amber_value_widget_create(
        scr->root, font_xl, 137.0f, 219.0f, 3, "300");
    if (scr->distance == NULL) goto fail;

    scr->distance_unit = amber_value_widget_create(
        scr->root, font_caption, 218.0f, 229.0f, 1, "M");
    if (scr->distance_unit == NULL) goto fail;

    scr->street = amber_ui_label_create(
        scr->root, font_instruction, ui_theme_amber_dim(), "RUE DES LILAS", 0.0f);
    if (scr->street == NULL) goto fail;
    amber_ui_place_centered(scr->street, scr->root, NAV_CX, 276.0f,
                            0.0f, 0.0f);

    scr->arrival = amber_ui_label_create(
        scr->root, font_caption, ui_theme_amber_bright(), "ETA 14:32", 0.0f);
    if (scr->arrival == NULL) goto fail;
    amber_ui_place_centered(scr->arrival, scr->root, NAV_CX, 302.0f,
                            0.0f, 0.0f);

    return scr;

fail:
    navigation_screen_destroy(scr);
    return NULL;
}

void navigation_screen_update(navigation_screen_t *scr, const ecu_data_t *data) {
    // ecu_data_t ne contient pas encore de navigation. Le scénario de démo est
    // stable et ne doit pas être invalidé à chaque lecture ECU.
    (void)scr;
    (void)data;
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
