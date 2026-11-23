#include "ui/icons/dash_icons.h"

#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"

// La couleur n'est pas copiée ici : elle vit dans le style local
// LV_STYLE_LINE_COLOR de l'objet, que ui_theme_restyle_tree sait remapper lors
// d'une bascule de palette.
typedef struct {
    dash_icon_type_t type;
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

static void draw_arc(lv_layer_t *layer, lv_color_t color, lv_coord_t width,
                     lv_coord_t cx, lv_coord_t cy, lv_coord_t r_mid,
                     float start_deg, float end_deg) {
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.color = color;
    d.opa = LV_OPA_COVER;
    d.width = width;
    d.center.x = cx;
    d.center.y = cy;
    d.radius = r_mid + width / 2;
    d.start_angle = start_deg;
    d.end_angle = end_deg;
    d.rounded = 1;
    amber_draw_arc_clipped(layer, &d);
}

/* Onde sinusoïdale douce composée de 3 arcs tangents (crête - creux - crête) */
static void draw_wave(lv_layer_t *layer, const lv_area_t *a, lv_color_t color,
                      lv_coord_t lw, int y_mid) {
    const lv_coord_t r = sw(a, 125);
    const int dy = 72;
    draw_arc(layer, color, lw, sx(a, 295), sy(a, y_mid + dy), r, 215.0f,
             325.0f);
    draw_arc(layer, color, lw, sx(a, 500), sy(a, y_mid - dy), r, 35.0f, 145.0f);
    draw_arc(layer, color, lw, sx(a, 705), sy(a, y_mid + dy), r, 215.0f,
             325.0f);
}

/* -------------------- 1. COOLANT (Thermomètre + vagues) -------------------- */

static void draw_coolant(lv_layer_t *layer, const lv_area_t *a,
                         lv_color_t color) {
    const lv_coord_t lw = sw(a, 58);

    /* Tige du thermomètre */
    draw_line(layer, color, lw, sx(a, 450), sy(a, 135), sx(a, 450), sy(a, 550));

    /* Bulbe inférieur */
    draw_circle_fill(layer, color, sx(a, 450), sy(a, 590), sw(a, 85));

    /* Graduations horizontales à droite (extrémités arrondies) */
    draw_line(layer, color, lw, sx(a, 450), sy(a, 210), sx(a, 610), sy(a, 210));
    draw_line(layer, color, lw, sx(a, 450), sy(a, 320), sx(a, 570), sy(a, 320));
    draw_line(layer, color, lw, sx(a, 450), sy(a, 430), sx(a, 610), sy(a, 430));

    /* Deux vagues ondulées courbes superposées */
    draw_wave(layer, a, color, lw, 745);
    draw_wave(layer, a, color, lw, 870);
}

/* -------------------- 2. BATTERY (Cadre + bornes + symboles -/+) ----------- */

static void draw_battery(lv_layer_t *layer, const lv_area_t *a,
                         lv_color_t color) {
    const lv_coord_t lw = sw(a, 55);

    /* Boîtier principal aux coins généreusement arrondis */
    draw_rect_outline(layer, color, lw, sw(a, 105), sx(a, 150), sy(a, 315),
                      sx(a, 850), sy(a, 780));

    /* Bornes supérieures adoucies */
    draw_rect_fill(layer, color, sw(a, 35), sx(a, 250), sy(a, 225), sx(a, 380),
                   sy(a, 325));
    draw_rect_fill(layer, color, sw(a, 35), sx(a, 620), sy(a, 225), sx(a, 750),
                   sy(a, 325));

    /* Symbole négatif (-) à gauche */
    draw_line(layer, color, lw, sx(a, 275), sy(a, 550), sx(a, 415), sy(a, 550));

    /* Symbole positif (+) à droite */
    draw_line(layer, color, lw, sx(a, 585), sy(a, 550), sx(a, 725), sy(a, 550));
    draw_line(layer, color, lw, sx(a, 655), sy(a, 480), sx(a, 655), sy(a, 620));
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

/* -------------------- 3b. FUEL (Pompe à essence automobile) --------------- */

static void draw_fuel(lv_layer_t *layer, const lv_area_t *a, lv_color_t color) {
    const lv_coord_t lw = sw(a, 55);
    const lv_coord_t x_body_l = sx(a, 185);
    const lv_coord_t x_body_r = sx(a, 605);
    const lv_coord_t y_body_t = sy(a, 210);
    const lv_coord_t y_body_b = sy(a, 775);

    /* Corps du distributeur aux coins adoucis + séparation du cadran */
    draw_rect_outline(layer, color, lw, sw(a, 95), x_body_l, y_body_t, x_body_r,
                      y_body_b);
    draw_line(layer, color, lw, x_body_l + lw / 2, sy(a, 465),
              x_body_r - lw / 2, sy(a, 465));

    /* Socle inférieur arrondi */
    draw_line(layer, color, sw(a, 62), sx(a, 145), y_body_b, sx(a, 645),
              y_body_b);

    /* Flexible latéral à boucle inférieure arrondie et pistolet */
    const lv_coord_t cx_loop = sx(a, 755);
    const lv_coord_t y_loop = sy(a, 635);
    const lv_coord_t r_out = sw(a, 105);
    const lv_coord_t x_hose_l = cx_loop - r_out + lw / 2;
    const lv_coord_t x_hose_r = cx_loop + r_out - (lw - lw / 2);
    const lv_coord_t y_attach = sy(a, 515);
    const lv_coord_t y_nozzle = sy(a, 345);

    draw_line(layer, color, lw, x_body_r - lw / 2, y_attach, x_hose_l,
              y_attach);
    draw_line(layer, color, lw, x_hose_l, y_attach, x_hose_l, y_loop);

    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = color;
    arc.opa = LV_OPA_COVER;
    arc.width = lw;
    arc.center.x = cx_loop;
    arc.center.y = y_loop;
    arc.radius = r_out;
    arc.start_angle = 0.0f;
    arc.end_angle = 180.0f;
    arc.rounded = 1;
    amber_draw_arc_clipped(layer, &arc);

    draw_line(layer, color, lw, x_hose_r, y_loop, x_hose_r, y_nozzle);
    draw_line(layer, color, lw, x_hose_r, y_nozzle, sx(a, 710), sy(a, 245));
}

/* -------------------- 4. OBD LINK (Maillons de chaîne) -------------------- */

static void draw_obd_link(lv_layer_t *layer, const lv_area_t *a,
                          lv_color_t color) {
    const lv_coord_t lw = sw(a, 58);
    const lv_coord_t r_out = sw(a, 175);
    const lv_coord_t cy = sy(a, 500);
    const lv_coord_t y_top = cy - r_out + lw / 2;
    const lv_coord_t y_bot = cy + r_out - (lw - lw / 2);

    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = color;
    arc.opa = LV_OPA_COVER;
    arc.width = lw;
    arc.center.y = cy;
    arc.radius = r_out;
    arc.rounded = 1;

    /* Demi-maillon arrondi gauche */
    const lv_coord_t cx_l = sx(a, 265);
    arc.center.x = cx_l;
    arc.start_angle = 90.0f;
    arc.end_angle = 270.0f;
    amber_draw_arc_clipped(layer, &arc);
    draw_line(layer, color, lw, cx_l, y_top, sx(a, 440), y_top);
    draw_line(layer, color, lw, cx_l, y_bot, sx(a, 440), y_bot);

    /* Demi-maillon arrondi droit */
    const lv_coord_t cx_r = sx(a, 735);
    arc.center.x = cx_r;
    arc.start_angle = 270.0f;
    arc.end_angle = 360.0f;
    amber_draw_arc_clipped(layer, &arc);
    arc.start_angle = 0.0f;
    arc.end_angle = 90.0f;
    amber_draw_arc_clipped(layer, &arc);
    draw_line(layer, color, lw, sx(a, 560), y_top, cx_r, y_top);
    draw_line(layer, color, lw, sx(a, 560), y_bot, cx_r, y_bot);

    /* Barre de jonction centrale */
    draw_line(layer, color, lw, sx(a, 355), cy, sx(a, 645), cy);
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
    const lv_color_t color = lv_obj_get_style_line_color(obj, LV_PART_MAIN);

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
        draw_coolant(layer, &a, color);
        break;
    case DASH_ICON_BATTERY:
        draw_battery(layer, &a, color);
        break;
    case DASH_ICON_OIL:
        draw_oil(layer, &a, color);
        break;
    case DASH_ICON_FUEL:
        draw_fuel(layer, &a, color);
        break;
    case DASH_ICON_ENGINE:
        draw_engine(layer, &a, color);
        break;
    case DASH_ICON_OBD_LINK:
        draw_obd_link(layer, &a, color);
        break;
    case DASH_ICON_OBD_DISCONNECTED:
        draw_obd_disconnected(layer, &a, color);
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
    lv_obj_set_style_line_color(obj, color, LV_PART_MAIN);

    lv_obj_set_user_data(obj, data);

    lv_obj_add_event_cb(obj, dash_icon_draw_event, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(obj, dash_icon_delete_event, LV_EVENT_DELETE, NULL);

    lv_obj_invalidate(obj);
    return obj;
}

void dash_icon_set_color(lv_obj_t *obj, lv_color_t color) {
    if (!obj) return;

    dash_icon_data_t *data = lv_obj_get_user_data(obj);
    if (!data ||
        lv_color_eq(lv_obj_get_style_line_color(obj, LV_PART_MAIN), color)) {
        return;
    }

    // Couleur : propriété de style réelle -> lv_obj invalide l'objet.
    lv_obj_set_style_line_color(obj, color, LV_PART_MAIN);
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
