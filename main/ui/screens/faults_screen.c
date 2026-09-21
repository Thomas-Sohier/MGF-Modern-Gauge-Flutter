#include "ui/screens/faults_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>

// Repère logique commun au cadran ambre : 320 unités sur le diamètre utile.
#define SCREEN_CX 160.0f
#define SCREEN_CY 160.0f
#define OUTER_R  153.0f
#define STATUS_R 137.0f
#define CORE_R   101.0f

#define STATUS_SEGMENTS 24
#define STATUS_GAP_DEG 2.4f
#define STATUS_BAR_DEG ((360.0f - STATUS_SEGMENTS * STATUS_GAP_DEG) / STATUS_SEGMENTS)

#define LINE_W      0.8f
#define ICON_LINE_W 2.0f

struct faults_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;
    lv_obj_t *title;
    lv_obj_t *subtitle;
    lv_obj_t *connected_head;
    lv_obj_t *connected_body;
    lv_obj_t *connected_foot;
    lv_obj_t *offline_head;
    lv_obj_t *offline_body;
    lv_obj_t *offline_foot;
    bool connected;
};

static void draw_ring_segments(lv_layer_t *layer, const ui_layout_t *layout,
                               bool connected) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    // Le cercle extérieur est une couronne de télémétrie, pas un simple cadre.
    // Son état plein / éteint reste lisible sans introduire de vert ou de rouge.
    for (int i = 0; i < STATUS_SEGMENTS; i++) {
        const float start = (float)i * (360.0f / STATUS_SEGMENTS) +
                            STATUS_GAP_DEG * 0.5f;
        const float end = start + STATUS_BAR_DEG;
        amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, OUTER_R, 2.0f,
                       start, end, connected ? bright : dim, false);
    }

    // Deux filets continus donnent de la profondeur sans fermer les panneaux.
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, STATUS_R, LINE_W,
                   4.0f, 176.0f, ui_theme_amber_separator(), false);
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, STATUS_R, LINE_W,
                   184.0f, 356.0f, ui_theme_amber_separator(), false);
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, CORE_R, LINE_W,
                   12.0f, 168.0f, ui_theme_amber_separator(), false);
    amber_draw_arc(layer, layout, SCREEN_CX, SCREEN_CY, CORE_R, LINE_W,
                   192.0f, 348.0f, ui_theme_amber_separator(), false);
}

static void draw_connector(lv_layer_t *layer, const ui_layout_t *layout,
                           bool connected) {
    const lv_color_t color = connected ? ui_theme_amber_bright()
                                       : ui_theme_amber_dim();
    const lv_color_t accent = connected ? ui_theme_amber_bright()
                                        : ui_theme_amber_separator();
    ui_layout_t line_layout = *layout;
    line_layout.ox -= 0.5f;
    line_layout.oy -= 0.5f;
    const float x1 = 139.0f;
    const float x2 = 181.0f;
    const float y1 = 111.0f;
    const float y2 = 145.0f;

    // Prise OBD stylisée : châssis, détrompeur et trois contacts.
    lv_draw_rect_dsc_t box;
    lv_draw_rect_dsc_init(&box);
    box.bg_opa = LV_OPA_TRANSP;
    box.border_opa = LV_OPA_COVER;
    box.border_color = color;
    box.border_width = LV_MAX(1, (int32_t)lroundf(ICON_LINE_W * layout->scale));
    box.radius = lroundf(4.0f * layout->scale);
    lv_area_t area = {
        lroundf(ui_layout_x(layout, x1)), lroundf(ui_layout_y(layout, y1)),
        lroundf(ui_layout_x(layout, x2)), lroundf(ui_layout_y(layout, y2))
    };
    lv_draw_rect(layer, &box, &area);

    amber_draw_line(layer, &line_layout, 147.0f, 103.0f, 147.0f, 111.0f,
                    ICON_LINE_W, color, false);
    amber_draw_line(layer, &line_layout, 160.0f, 103.0f, 160.0f, 111.0f,
                    ICON_LINE_W, color, false);
    amber_draw_line(layer, &line_layout, 173.0f, 103.0f, 173.0f, 111.0f,
                    ICON_LINE_W, color, false);

    amber_draw_line(layer, &line_layout, 147.0f, 120.0f, 147.0f, 128.0f,
                    ICON_LINE_W, color, true);
    amber_draw_line(layer, &line_layout, 160.0f, 120.0f, 160.0f, 128.0f,
                    ICON_LINE_W, color, true);
    amber_draw_line(layer, &line_layout, 173.0f, 120.0f, 173.0f, 128.0f,
                    ICON_LINE_W, color, true);

    // Le câble est ouvert côté déconnexion et fermé côté liaison active.
    amber_draw_line(layer, &line_layout, x2, 128.0f, 193.0f, 128.0f,
                    ICON_LINE_W, accent, true);
    amber_draw_line(layer, &line_layout, 193.0f, 128.0f, 198.0f, 133.0f,
                    ICON_LINE_W, accent, true);
    if (!connected) {
        amber_draw_line(layer, &line_layout, 128.0f, 99.0f, 192.0f, 157.0f,
                        ICON_LINE_W, ui_theme_amber_separator(), true);
    }
}

static void canvas_draw_cb(lv_event_t *event) {
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    faults_screen_t *scr = lv_obj_get_user_data(canvas);
    if (scr == NULL || layer == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    ui_layout_t layout = amber_draw_layout(&area);
    layout.ox += area.x1;
    layout.oy += area.y1;
    ui_layout_t line_layout = layout;
    line_layout.ox -= 0.5f;
    line_layout.oy -= 0.5f;
    draw_ring_segments(layer, &layout, scr->connected);

    // Trait médian discret : il sépare le statut de liaison de l'information
    // de diagnostic, avec une rupture centrale pour conserver de l'air.
    amber_draw_line(layer, &line_layout, 61.0f, 231.0f, 143.0f, 231.0f,
                    LINE_W, ui_theme_amber_separator(), false);
    amber_draw_line(layer, &line_layout, 177.0f, 231.0f, 259.0f, 231.0f,
                    LINE_W, ui_theme_amber_separator(), false);
    amber_draw_line(layer, &line_layout, 76.0f, 237.0f, 76.0f, 242.0f,
                    LINE_W, ui_theme_amber_separator(), false);
    amber_draw_line(layer, &line_layout, 244.0f, 237.0f, 244.0f, 242.0f,
                    LINE_W, ui_theme_amber_separator(), false);

    draw_connector(layer, &layout, scr->connected);
}

static void set_state_visibility(faults_screen_t *scr, bool connected) {
    if (connected) {
        lv_obj_clear_flag(scr->connected_head, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scr->connected_body, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scr->connected_foot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scr->offline_head, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scr->offline_body, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scr->offline_foot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(scr->connected_head, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scr->connected_body, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(scr->connected_foot, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scr->offline_head, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scr->offline_body, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(scr->offline_foot, LV_OBJ_FLAG_HIDDEN);
    }
}

faults_screen_t *faults_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    faults_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    lv_obj_set_style_bg_color(parent, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_update_layout(parent);

    scr->root = amber_ui_root_create(parent);
    if (scr->root == NULL) goto fail;
    lv_obj_set_style_radius(scr->root, 0, 0);
    lv_obj_set_style_clip_corner(scr->root, false, 0);
    scr->canvas = amber_ui_canvas_create(scr->root, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    const lv_font_t *title_font = ui_font_or(ui_font_l, &lv_font_montserrat_20);
    const lv_font_t *body_font = ui_font_or(ui_font_m, &lv_font_montserrat_20);
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    scr->title = amber_ui_label_create(scr->root, title_font, bright,
                                        "CODES ERREURS", 0.0f);
    scr->subtitle = amber_ui_label_create(scr->root, body_font, dim,
                                          "DIAGNOSTIC ECU", 0.0f);
    scr->connected_head = amber_ui_label_create(
        scr->root, body_font, bright, "LIAISON ACTIVE", 0.0f);
    scr->connected_body = amber_ui_label_create(
        scr->root, body_font, bright, "DIAGNOSTIC PRET", 0.0f);
    scr->connected_foot = amber_ui_label_create(
        scr->root, body_font, dim, "CODES NON LUS", 0.0f);
    scr->offline_head = amber_ui_label_create(
        scr->root, body_font, dim, "LIAISON ABSENTE", 0.0f);
    scr->offline_body = amber_ui_label_create(
        scr->root, body_font, dim, "ECU NON CONNECTE", 0.0f);
    scr->offline_foot = amber_ui_label_create(
        scr->root, body_font, bright, "RECONNECTER L ECU", 0.0f);

    if (scr->title == NULL || scr->subtitle == NULL ||
        scr->connected_head == NULL || scr->connected_body == NULL ||
        scr->connected_foot == NULL || scr->offline_head == NULL ||
        scr->offline_body == NULL || scr->offline_foot == NULL) goto fail;

    amber_ui_place_centered(scr->title, scr->root, SCREEN_CX, 35.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->subtitle, scr->root, SCREEN_CX, 59.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->connected_head, scr->root, SCREEN_CX, 176.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->connected_body, scr->root, SCREEN_CX, 201.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->connected_foot, scr->root, SCREEN_CX, 264.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->offline_head, scr->root, SCREEN_CX, 176.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->offline_body, scr->root, SCREEN_CX, 201.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->offline_foot, scr->root, SCREEN_CX, 264.0f,
                            0.0f, 0.0f);

    scr->connected = false;
    set_state_visibility(scr, false);
    return scr;

fail:
    faults_screen_destroy(scr);
    return NULL;
}

void faults_screen_update(faults_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL || scr->canvas == NULL) return;
    if (scr->connected == data->connected) return;

    scr->connected = data->connected;
    set_state_visibility(scr, scr->connected);
    lv_obj_invalidate(scr->canvas);
}

void faults_screen_destroy(faults_screen_t *scr) {
    if (scr == NULL) return;
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
