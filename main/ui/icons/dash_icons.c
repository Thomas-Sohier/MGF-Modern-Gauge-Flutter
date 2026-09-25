#include "ui/icons/dash_icons.h"

#include "ui/themes/ui_theme.h"

typedef struct {
    dash_icon_type_t type;
    lv_color_t color;
} dash_icon_data_t;

static lv_coord_t sx(const lv_area_t *a, int n) {
    const int w = lv_area_get_width(a);
    return a->x1 + (lv_coord_t)((w * n) / 1000);
}

static lv_coord_t sy(const lv_area_t *a, int n) {
    const int h = lv_area_get_height(a);
    return a->y1 + (lv_coord_t)((h * n) / 1000);
}

static lv_coord_t sw(const lv_area_t *a, int n) {
    const int w = lv_area_get_width(a);
    lv_coord_t v = (lv_coord_t)((w * n) / 1000);
    return v < 1 ? 1 : v;
}

static void draw_line(lv_layer_t *layer, lv_color_t color, lv_coord_t width,
                      lv_coord_t x1, lv_coord_t y1, lv_coord_t x2,
                      lv_coord_t y2) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = color;
    d.width = width;
    d.round_start = 1;
    d.round_end = 1;

    lv_point_precise_t p1 = {x1, y1};
    lv_point_precise_t p2 = {x2, y2};
    d.p1 = p1;
    d.p2 = p2;

    lv_draw_line(layer, &d);
}

static void draw_rect_outline(lv_layer_t *layer, lv_color_t color,
                              lv_coord_t width, lv_coord_t radius,
                              lv_coord_t x1, lv_coord_t y1, lv_coord_t x2,
                              lv_coord_t y2) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.border_opa = LV_OPA_COVER;
    d.border_color = color;
    d.border_width = width;
    d.radius = radius;

    lv_area_t r = {x1, y1, x2, y2};
    lv_draw_rect(layer, &d, &r);
}

static void draw_rect_fill(lv_layer_t *layer, lv_color_t color,
                           lv_coord_t radius, lv_coord_t x1, lv_coord_t y1,
                           lv_coord_t x2, lv_coord_t y2) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_COVER;
    d.bg_color = color;
    d.border_width = 0;
    d.radius = radius;

    lv_area_t r = {x1, y1, x2, y2};
    lv_draw_rect(layer, &d, &r);
}

static void draw_circle_fill(lv_layer_t *layer, lv_color_t color, lv_coord_t cx,
                             lv_coord_t cy, lv_coord_t r) {
    draw_rect_fill(layer, color, LV_RADIUS_CIRCLE, cx - r, cy - r, cx + r,
                   cy + r);
}

/* -------------------- 1. COOLANT (Thermomètre + vagues) -------------------- */

static void draw_coolant(lv_layer_t *layer, const lv_area_t *a,
                         lv_color_t color) {
    const lv_coord_t lw = sw(a, 60);

    /* Tige du thermomètre */
    draw_line(layer, color, lw, sx(a, 410), sy(a, 130), sx(a, 410), sy(a, 540));

    /* Bulbe inférieur */
    draw_circle_fill(layer, color, sx(a, 410), sy(a, 595), sw(a, 85));

    /* Graduations horizontales à droite */
    draw_line(layer, color, lw, sx(a, 410), sy(a, 210), sx(a, 560), sy(a, 210));
    draw_line(layer, color, lw, sx(a, 410), sy(a, 320), sx(a, 510), sy(a, 320));
    draw_line(layer, color, lw, sx(a, 410), sy(a, 430), sx(a, 560), sy(a, 430));

    /* Deux vagues ondulées superposées */
    const int wave_y[2] = {750, 875};
    for (int k = 0; k < 2; k++) {
        const int y = wave_y[k];
        const int dy = 40;
        draw_line(layer, color, lw, sx(a, 160), sy(a, y), sx(a, 270),
                  sy(a, y - dy));
        draw_line(layer, color, lw, sx(a, 270), sy(a, y - dy), sx(a, 380),
                  sy(a, y + dy));
        draw_line(layer, color, lw, sx(a, 380), sy(a, y + dy), sx(a, 490),
                  sy(a, y - dy));
        draw_line(layer, color, lw, sx(a, 490), sy(a, y - dy), sx(a, 600),
                  sy(a, y + dy));
        draw_line(layer, color, lw, sx(a, 600), sy(a, y + dy), sx(a, 710),
                  sy(a, y - dy));
        draw_line(layer, color, lw, sx(a, 710), sy(a, y - dy), sx(a, 820),
                  sy(a, y));
    }
}

/* -------------------- 2. BATTERY (Cadre + bornes + symboles -/+) ----------- */

static void draw_battery(lv_layer_t *layer, const lv_area_t *a,
                         lv_color_t color) {
    const lv_coord_t lw = sw(a, 55);

    /* Boîtier principal */
    draw_rect_outline(layer, color, lw, sw(a, 35), sx(a, 150), sy(a, 320),
                      sx(a, 850), sy(a, 780));

    /* Bornes supérieures */
    draw_rect_fill(layer, color, sw(a, 15), sx(a, 240), sy(a, 230), sx(a, 370),
                   sy(a, 320));
    draw_rect_fill(layer, color, sw(a, 15), sx(a, 630), sy(a, 230), sx(a, 760),
                   sy(a, 320));

    /* Symbole négatif (-) à gauche */
    draw_line(layer, color, lw, sx(a, 270), sy(a, 550), sx(a, 420), sy(a, 550));

    /* Symbole positif (+) à droite */
    draw_line(layer, color, lw, sx(a, 580), sy(a, 550), sx(a, 730), sy(a, 550));
    draw_line(layer, color, lw, sx(a, 655), sy(a, 475), sx(a, 655), sy(a, 625));
}

/* -------------------- 3. OIL (Burette d'huile automobile) ------------------ */

static void draw_oil(lv_layer_t *layer, const lv_area_t *a, lv_color_t color) {
    const lv_coord_t lw = sw(a, 55);

    /* Base plate et corps */
    draw_line(layer, color, lw, sx(a, 280), sy(a, 720), sx(a, 620),
              sy(a, 720)); // bas
    draw_line(layer, color, lw, sx(a, 280), sy(a, 720), sx(a, 280),
              sy(a, 500)); // flanc gauche
    draw_line(layer, color, lw, sx(a, 280), sy(a, 500), sx(a, 490),
              sy(a, 500)); // épaule haute
    draw_line(layer, color, lw, sx(a, 490), sy(a, 500), sx(a, 620),
              sy(a, 610)); // pente vers le bec
    draw_line(layer, color, lw, sx(a, 620), sy(a, 610), sx(a, 620),
              sy(a, 720)); // flanc droit

    /* Anse arrière (poignée) */
    draw_line(layer, color, lw, sx(a, 280), sy(a, 520), sx(a, 150), sy(a, 440));
    draw_line(layer, color, lw, sx(a, 150), sy(a, 440), sx(a, 150), sy(a, 610));
    draw_line(layer, color, lw, sx(a, 150), sy(a, 610), sx(a, 280), sy(a, 670));

    /* Bouchon de remplissage en T */
    draw_line(layer, color, lw, sx(a, 390), sy(a, 500), sx(a, 390),
              sy(a, 390)); // tige
    draw_line(layer, color, lw, sx(a, 330), sy(a, 390), sx(a, 450),
              sy(a, 390)); // chapeau

    /* Long bec verseur */
    draw_line(layer, color, lw, sx(a, 600), sy(a, 590), sx(a, 830), sy(a, 400));
    draw_line(layer, color, lw, sx(a, 830), sy(a, 400), sx(a, 885),
              sy(a, 415)); // pointe

    /* Goutte d'huile sous le bec */
    draw_circle_fill(layer, color, sx(a, 865), sy(a, 570), sw(a, 42));
    draw_line(layer, color, lw, sx(a, 865), sy(a, 520), sx(a, 865), sy(a, 560));
}

/* -------------------- 4. OBD LINK (Maillons de chaîne) -------------------- */

static void draw_obd_link(lv_layer_t *layer, const lv_area_t *a,
                          lv_color_t color) {
    const lv_coord_t lw = sw(a, 60);

    /*
     * Deux capsules/stades arrondis horizontaux (radius = LV_RADIUS_CIRCLE)
     * traversés par une barre centrale pour former les maillons entrelacés.
     */
    const lv_coord_t y1 = sy(a, 360);
    const lv_coord_t y2 = sy(a, 640);

    /* Maillon gauche (capsule creuse) */
    draw_rect_outline(layer, color, lw, LV_RADIUS_CIRCLE, sx(a, 110), y1,
                      sx(a, 520), y2);

    /* Maillon droit (capsule creuse) */
    draw_rect_outline(layer, color, lw, LV_RADIUS_CIRCLE, sx(a, 480), y1,
                      sx(a, 890), y2);

    /* Barre de jonction centrale */
    draw_line(layer, color, lw, sx(a, 340), sy(a, 500), sx(a, 660), sy(a, 500));
}

/* Maillons barres : meme chaine (attenuee) + barre diagonale ambre vif. */
static void draw_obd_disconnected(lv_layer_t *layer, const lv_area_t *a,
                                  lv_color_t color) {
    draw_obd_link(layer, a, color);

    const lv_coord_t lw = sw(a, 85);
    draw_line(layer, ui_theme_amber_bright(), lw, sx(a, 130), sy(a, 840),
              sx(a, 870), sy(a, 160));
}

/* -------------------- 5. ENGINE (Check engine optionnel) ------------------- */

static void draw_engine(lv_layer_t *layer, const lv_area_t *a,
                        lv_color_t color) {
    const lv_coord_t lw = sw(a, 55);

    const int pts[][2] = {{230, 350}, {320, 350}, {390, 265}, {665, 265},
                          {750, 350}, {845, 350}, {845, 665}, {760, 665},
                          {705, 730}, {355, 730}, {290, 665}, {230, 665},
                          {230, 350}};

    for (unsigned i = 0; i + 1 < sizeof(pts) / sizeof(pts[0]); i++) {
        draw_line(layer, color, lw, sx(a, pts[i][0]), sy(a, pts[i][1]),
                  sx(a, pts[i + 1][0]), sy(a, pts[i + 1][1]));
    }

    draw_line(layer, color, lw, sx(a, 470), sy(a, 265), sx(a, 470), sy(a, 175));
    draw_line(layer, color, lw, sx(a, 400), sy(a, 175), sx(a, 545), sy(a, 175));

    draw_line(layer, color, lw, sx(a, 230), sy(a, 430), sx(a, 145), sy(a, 430));
    draw_line(layer, color, lw, sx(a, 145), sy(a, 350), sx(a, 145), sy(a, 575));
    draw_line(layer, color, lw, sx(a, 145), sy(a, 575), sx(a, 230), sy(a, 575));

    draw_line(layer, color, lw, sx(a, 845), sy(a, 445), sx(a, 915), sy(a, 445));
    draw_line(layer, color, lw, sx(a, 915), sy(a, 445), sx(a, 915), sy(a, 575));
    draw_line(layer, color, lw, sx(a, 915), sy(a, 575), sx(a, 845), sy(a, 575));
}

/* -------------------- GESTION DU WIDGET LVGL -------------------- */

static void dash_icon_draw_event(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *obj = lv_event_get_target(e);
    dash_icon_data_t *data = lv_obj_get_user_data(obj);
    if (!data) return;

    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t a;
    lv_obj_get_coords(obj, &a);

    /* Marge interne pour éviter tout écornage lors de l'anti-aliasing */
    lv_coord_t pad = sw(&a, 50);
    a.x1 += pad;
    a.y1 += pad;
    a.x2 -= pad;
    a.y2 -= pad;

    switch (data->type) {
    case DASH_ICON_COOLANT:
        draw_coolant(layer, &a, data->color);
        break;
    case DASH_ICON_BATTERY:
        draw_battery(layer, &a, data->color);
        break;
    case DASH_ICON_OIL:
        draw_oil(layer, &a, data->color);
        break;
    case DASH_ICON_ENGINE:
        draw_engine(layer, &a, data->color);
        break;
    case DASH_ICON_OBD_LINK:
        draw_obd_link(layer, &a, data->color);
        break;
    case DASH_ICON_OBD_DISCONNECTED:
        draw_obd_disconnected(layer, &a, data->color);
        break;
    default:
        break;
    }
}

static void dash_icon_delete_event(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    dash_icon_data_t *data = lv_obj_get_user_data(obj);

    if (data) {
        lv_free(data);
        lv_obj_set_user_data(obj, NULL);
    }
}

lv_obj_t *dash_icon_create(lv_obj_t *parent, dash_icon_type_t type,
                           lv_coord_t size, lv_color_t color) {
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, size, size);
    // Décoratif : les appuis doivent atteindre le navigateur du dashboard.
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    dash_icon_data_t *data = lv_malloc(sizeof(dash_icon_data_t));
    if (!data) {
        lv_obj_delete(obj);
        return NULL;
    }

    data->type = type;
    data->color = color;

    lv_obj_set_user_data(obj, data);

    lv_obj_add_event_cb(obj, dash_icon_draw_event, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(obj, dash_icon_delete_event, LV_EVENT_DELETE, NULL);

    lv_obj_invalidate(obj);
    return obj;
}

void dash_icon_set_color(lv_obj_t *obj, lv_color_t color) {
    if (!obj) return;

    dash_icon_data_t *data = lv_obj_get_user_data(obj);
    if (!data || lv_color_eq(data->color, color)) return;

    data->color = color;
    lv_obj_invalidate(obj);
}

void dash_icon_set_type(lv_obj_t *obj, dash_icon_type_t type) {
    if (!obj) return;

    dash_icon_data_t *data = lv_obj_get_user_data(obj);
    if (!data || data->type == type) return;

    data->type = type;
    lv_obj_invalidate(obj);
}

void dash_icon_set_size(lv_obj_t *obj, lv_coord_t size) {
    if (!obj) return;
    lv_obj_set_size(obj, size, size);
    lv_obj_invalidate(obj);
}
