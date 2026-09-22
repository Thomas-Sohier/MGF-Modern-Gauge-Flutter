#include "ui/screens/faults_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"

#include <stdio.h>
#include <string.h>

// Repère logique commun au cadran ambre : 320 unités sur le diamètre utile.
#define SCREEN_CX 160.0f

#define ICON_LINE_W 2.6f

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
    bool has_snapshot;
    bool faults_available;
    uint8_t fault_flags;
    char body_text[24];
    char foot_text[64];
};

// Libellés courts : la liste doit tenir sur deux lignes dans le cadran rond.
static const struct {
    uint8_t flag;
    const char *label;
} kFaults[] = {
    {ECU_FAULT_COOLANT_SENSOR, "SONDE EAU"},
    {ECU_FAULT_INTAKE_AIR_SENSOR, "SONDE AIR"},
    {ECU_FAULT_FUEL_PUMP, "POMPE ESS."},
    {ECU_FAULT_THROTTLE_POT, "PAPILLON"},
};
#define FAULT_KIND_COUNT (sizeof(kFaults) / sizeof(kFaults[0]))

static void draw_engine_icon(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t detail = ui_theme_amber_separator();

    // Pictogramme moteur agrandi : sa forme reste un indicateur visuel, pas un
    // diagnostic. La couleur ambre ne prétend donc pas signaler un défaut.
    const float body[][2] = {
        {112.0f, 105.0f}, {124.0f, 105.0f}, {133.0f, 94.0f},
        {187.0f, 94.0f}, {196.0f, 105.0f}, {208.0f, 105.0f},
        {208.0f, 145.0f}, {197.0f, 145.0f}, {189.0f, 155.0f},
        {131.0f, 155.0f}, {123.0f, 145.0f}, {112.0f, 145.0f},
        {112.0f, 105.0f}
    };
    for (unsigned i = 0; i + 1 < sizeof(body) / sizeof(body[0]); i++) {
        amber_draw_line(layer, layout, body[i][0], body[i][1],
                        body[i + 1][0], body[i + 1][1], ICON_LINE_W,
                        bright, false);
    }

    // Collecteur supérieur et petites branches latérales, dessinés en marge
    // du bloc pour conserver la silhouette immédiatement reconnaissable.
    amber_draw_line(layer, layout, 154.0f, 94.0f, 154.0f, 82.0f,
                    ICON_LINE_W, bright, false);
    amber_draw_line(layer, layout, 154.0f, 82.0f, 170.0f, 82.0f,
                    ICON_LINE_W, bright, false);
    amber_draw_line(layer, layout, 170.0f, 82.0f, 170.0f, 94.0f,
                    ICON_LINE_W, bright, false);

    amber_draw_line(layer, layout, 112.0f, 116.0f, 103.0f, 116.0f,
                    ICON_LINE_W, bright, false);
    amber_draw_line(layer, layout, 103.0f, 116.0f, 103.0f, 134.0f,
                    ICON_LINE_W, bright, false);
    amber_draw_line(layer, layout, 103.0f, 134.0f, 112.0f, 134.0f,
                    ICON_LINE_W, bright, false);
    amber_draw_line(layer, layout, 208.0f, 116.0f, 217.0f, 116.0f,
                    ICON_LINE_W, bright, false);
    amber_draw_line(layer, layout, 217.0f, 116.0f, 217.0f, 134.0f,
                    ICON_LINE_W, bright, false);
    amber_draw_line(layer, layout, 217.0f, 134.0f, 208.0f, 134.0f,
                    ICON_LINE_W, bright, false);

    // Un seul détail intérieur donne du relief sans ajouter de couronne ou de
    // grille décorative autour du statut.
    amber_draw_line(layer, layout, 140.0f, 116.0f, 180.0f, 116.0f,
                    ICON_LINE_W, detail, false);
    amber_draw_line(layer, layout, 180.0f, 116.0f, 180.0f, 136.0f,
                    ICON_LINE_W, detail, false);
    amber_draw_line(layer, layout, 180.0f, 136.0f, 140.0f, 136.0f,
                    ICON_LINE_W, detail, false);
    amber_draw_line(layer, layout, 140.0f, 136.0f, 140.0f, 116.0f,
                    ICON_LINE_W, detail, false);
}

static void canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    faults_screen_t *scr = lv_obj_get_user_data(canvas);
    if (scr == NULL || layer == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);
    draw_engine_icon(layer, &layout);
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

    scr->root = amber_ui_root_create(parent);
    if (scr->root == NULL) goto fail;
    scr->canvas = amber_ui_canvas_create(scr->root, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    const lv_font_t *title_font = amber_ui_font_value();
    const lv_font_t *status_font = amber_ui_font_value();
    const lv_font_t *body_font = ui_font_or(ui_font_m, &lv_font_montserrat_20);
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    scr->title = amber_ui_label_create(scr->root, title_font, bright,
                                        "CODES ERREURS", 0.0f);
    scr->subtitle = amber_ui_label_create(scr->root, body_font, dim,
                                          "ETAT DIAGNOSTIC", 0.0f);
    scr->connected_head = amber_ui_label_create(
        scr->root, body_font, bright, "LIAISON ACTIVE", 0.0f);
    scr->connected_body = amber_ui_label_create(
        scr->root, status_font, bright, "DEFAUTS INCONNUS", 0.0f);
    scr->connected_foot = amber_ui_label_create(
        scr->root, body_font, dim, "CODES NON FOURNIS", 0.0f);
    scr->offline_head = amber_ui_label_create(
        scr->root, body_font, dim, "LIAISON ABSENTE", 0.0f);
    scr->offline_body = amber_ui_label_create(
        scr->root, status_font, bright, "DEFAUTS INCONNUS", 0.0f);
    scr->offline_foot = amber_ui_label_create(
        scr->root, body_font, dim, "ECU NON CONNECTE", 0.0f);

    if (scr->title == NULL || scr->subtitle == NULL ||
        scr->connected_head == NULL || scr->connected_body == NULL ||
        scr->connected_foot == NULL || scr->offline_head == NULL ||
        scr->offline_body == NULL || scr->offline_foot == NULL) goto fail;

    amber_ui_place_centered(scr->title, scr->root, SCREEN_CX, 28.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->subtitle, scr->root, SCREEN_CX, 53.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->connected_body, scr->root, SCREEN_CX, 183.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->connected_head, scr->root, SCREEN_CX, 216.0f,
                            0.0f, 0.0f);
    lv_obj_set_style_text_align(scr->connected_foot, LV_TEXT_ALIGN_CENTER, 0);
    amber_ui_place_centered(scr->connected_foot, scr->root, SCREEN_CX, 240.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->offline_body, scr->root, SCREEN_CX, 183.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->offline_head, scr->root, SCREEN_CX, 216.0f,
                            0.0f, 0.0f);
    amber_ui_place_centered(scr->offline_foot, scr->root, SCREEN_CX, 240.0f,
                            0.0f, 0.0f);

    scr->connected = false;
    set_state_visibility(scr, false);
    return scr;

fail:
    faults_screen_destroy(scr);
    return NULL;
}

static void format_faults(faults_screen_t *scr) {
    if (!scr->faults_available) {
        snprintf(scr->body_text, sizeof(scr->body_text), "DEFAUTS INCONNUS");
        snprintf(scr->foot_text, sizeof(scr->foot_text), "CODES NON FOURNIS");
        return;
    }

    unsigned count = 0;
    size_t used = 0;
    scr->foot_text[0] = '\0';
    for (size_t i = 0; i < FAULT_KIND_COUNT; i++) {
        if ((scr->fault_flags & kFaults[i].flag) == 0) continue;
        // Deux défauts par ligne au plus.
        const char *sep = count == 0 ? "" : (count % 2 == 0 ? "\n" : " / ");
        const int n = snprintf(scr->foot_text + used,
                               sizeof(scr->foot_text) - used, "%s%s", sep,
                               kFaults[i].label);
        if (n > 0 && (size_t)n < sizeof(scr->foot_text) - used) used += (size_t)n;
        count++;
    }
    if (count == 0) {
        snprintf(scr->body_text, sizeof(scr->body_text), "AUCUN DEFAUT");
        snprintf(scr->foot_text, sizeof(scr->foot_text), "CAPTEURS OK");
    } else {
        snprintf(scr->body_text, sizeof(scr->body_text), "%u DEFAUT%s", count,
                 count > 1 ? "S" : "");
    }
}

void faults_screen_update(faults_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL || scr->canvas == NULL) return;
    const bool available = data->connected && data->faults_available;
    const uint8_t flags = available ? data->fault_flags : 0;
    if (scr->has_snapshot && scr->connected == data->connected &&
        scr->faults_available == available && scr->fault_flags == flags) {
        return;
    }

    scr->has_snapshot = true;
    scr->connected = data->connected;
    scr->faults_available = available;
    scr->fault_flags = flags;
    if (scr->connected) {
        format_faults(scr);
        lv_label_set_text_static(scr->connected_body, scr->body_text);
        lv_label_set_text_static(scr->connected_foot, scr->foot_text);
        amber_ui_place_centered(scr->connected_body, scr->root, SCREEN_CX,
                                183.0f, 0.0f, 0.0f);
        // Une liste sur deux lignes descend d'une demi-ligne pour rester
        // centrée sous « LIAISON ACTIVE ».
        amber_ui_place_centered(scr->connected_foot, scr->root, SCREEN_CX,
                                strchr(scr->foot_text, '\n') ? 252.0f : 240.0f,
                                0.0f, 0.0f);
    }
    set_state_visibility(scr, scr->connected);
    lv_obj_invalidate(scr->canvas);
}

void faults_screen_destroy(faults_screen_t *scr) {
    if (scr == NULL) return;
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
