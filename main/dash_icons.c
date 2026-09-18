#include "dash_icons.h"

typedef struct {
    dash_icon_type_t type;
    lv_color_t color;
} dash_icon_data_t;

static lv_coord_t sx(const lv_area_t * a, int n)
{
    const int w = lv_area_get_width(a);
    return a->x1 + (lv_coord_t)((w * n) / 1000);
}

static lv_coord_t sy(const lv_area_t * a, int n)
{
    const int h = lv_area_get_height(a);
    return a->y1 + (lv_coord_t)((h * n) / 1000);
}

static lv_coord_t sw(const lv_area_t * a, int n)
{
    const int w = lv_area_get_width(a);
    lv_coord_t v = (lv_coord_t)((w * n) / 1000);
    return v < 1 ? 1 : v;
}

static void draw_line(lv_layer_t * layer,
                      lv_color_t color,
                      lv_coord_t width,
                      lv_coord_t x1, lv_coord_t y1,
                      lv_coord_t x2, lv_coord_t y2)
{
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = color;
    d.width = width;
    d.round_start = 1;
    d.round_end = 1;

    lv_point_precise_t p1 = { x1, y1 };
    lv_point_precise_t p2 = { x2, y2 };
    d.p1 = p1;
    d.p2 = p2;

    lv_draw_line(layer, &d);
}

static void draw_rect_outline(lv_layer_t * layer,
                              lv_color_t color,
                              lv_coord_t width,
                              lv_coord_t radius,
                              lv_coord_t x1, lv_coord_t y1,
                              lv_coord_t x2, lv_coord_t y2)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.border_opa = LV_OPA_COVER;
    d.border_color = color;
    d.border_width = width;
    d.radius = radius;

    lv_area_t r = { x1, y1, x2, y2 };
    lv_draw_rect(layer, &d, &r);
}

static void draw_rect_fill(lv_layer_t * layer,
                           lv_color_t color,
                           lv_coord_t radius,
                           lv_coord_t x1, lv_coord_t y1,
                           lv_coord_t x2, lv_coord_t y2)
{
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_COVER;
    d.bg_color = color;
    d.border_width = 0;
    d.radius = radius;

    lv_area_t r = { x1, y1, x2, y2 };
    lv_draw_rect(layer, &d, &r);
}

static void draw_circle_fill(lv_layer_t * layer,
                             lv_color_t color,
                             lv_coord_t cx, lv_coord_t cy,
                             lv_coord_t r)
{
    draw_rect_fill(layer, color, LV_RADIUS_CIRCLE,
                   cx - r, cy - r, cx + r, cy + r);
}

/* -------------------- COOLANT -------------------- */

static void draw_coolant(lv_layer_t * layer,
                         const lv_area_t * a,
                         lv_color_t color)
{
    const lv_coord_t lw = sw(a, 55);

    /* thermomètre */
    draw_line(layer, color, lw,
              sx(a, 405), sy(a, 170),
              sx(a, 405), sy(a, 600));

    draw_circle_fill(layer, color,
                     sx(a, 405), sy(a, 655),
                     sw(a, 95));

    /* graduations */
    draw_line(layer, color, lw,
              sx(a, 405), sy(a, 255),
              sx(a, 545), sy(a, 255));
    draw_line(layer, color, lw,
              sx(a, 405), sy(a, 365),
              sx(a, 520), sy(a, 365));
    draw_line(layer, color, lw,
              sx(a, 405), sy(a, 475),
              sx(a, 545), sy(a, 475));

    /* vagues */
    const int wave_y[2] = { 770, 885 };
    for(int k = 0; k < 2; k++) {
        int y = wave_y[k];
        draw_line(layer, color, lw,
                  sx(a, 140), sy(a, y),
                  sx(a, 250), sy(a, y - 45));
        draw_line(layer, color, lw,
                  sx(a, 250), sy(a, y - 45),
                  sx(a, 360), sy(a, y));
        draw_line(layer, color, lw,
                  sx(a, 360), sy(a, y),
                  sx(a, 470), sy(a, y + 45));
        draw_line(layer, color, lw,
                  sx(a, 470), sy(a, y + 45),
                  sx(a, 580), sy(a, y));
        draw_line(layer, color, lw,
                  sx(a, 580), sy(a, y),
                  sx(a, 690), sy(a, y - 45));
        draw_line(layer, color, lw,
                  sx(a, 690), sy(a, y - 45),
                  sx(a, 800), sy(a, y));
        draw_line(layer, color, lw,
                  sx(a, 800), sy(a, y),
                  sx(a, 900), sy(a, y + 35));
    }
}

/* -------------------- BATTERY -------------------- */

static void draw_battery(lv_layer_t * layer,
                         const lv_area_t * a,
                         lv_color_t color)
{
    const lv_coord_t lw = sw(a, 55);

    draw_rect_outline(layer, color, lw, sw(a, 30),
                      sx(a, 145), sy(a, 300),
                      sx(a, 855), sy(a, 720));

    /* bornes */
    draw_rect_fill(layer, color, sw(a, 18),
                   sx(a, 255), sy(a, 220),
                   sx(a, 355), sy(a, 300));
    draw_rect_fill(layer, color, sw(a, 18),
                   sx(a, 645), sy(a, 220),
                   sx(a, 745), sy(a, 300));

    /* - */
    draw_line(layer, color, lw,
              sx(a, 260), sy(a, 510),
              sx(a, 390), sy(a, 510));

    /* + */
    draw_line(layer, color, lw,
              sx(a, 610), sy(a, 510),
              sx(a, 750), sy(a, 510));
    draw_line(layer, color, lw,
              sx(a, 680), sy(a, 440),
              sx(a, 680), sy(a, 580));
}

/* -------------------- OIL -------------------- */

static void draw_oil(lv_layer_t * layer,
                     const lv_area_t * a,
                     lv_color_t color)
{
    const lv_coord_t lw = sw(a, 55);

    /* corps de la burette */
    draw_line(layer, color, lw,
              sx(a, 255), sy(a, 420),
              sx(a, 575), sy(a, 420));
    draw_line(layer, color, lw,
              sx(a, 575), sy(a, 420),
              sx(a, 685), sy(a, 540));
    draw_line(layer, color, lw,
              sx(a, 685), sy(a, 540),
              sx(a, 560), sy(a, 700));
    draw_line(layer, color, lw,
              sx(a, 560), sy(a, 700),
              sx(a, 255), sy(a, 700));
    draw_line(layer, color, lw,
              sx(a, 255), sy(a, 700),
              sx(a, 255), sy(a, 420));

    /* poignée */
    draw_line(layer, color, lw,
              sx(a, 255), sy(a, 430),
              sx(a, 120), sy(a, 350));
    draw_line(layer, color, lw,
              sx(a, 120), sy(a, 350),
              sx(a, 165), sy(a, 220));
    draw_line(layer, color, lw,
              sx(a, 165), sy(a, 220),
              sx(a, 335), sy(a, 305));
    draw_line(layer, color, lw,
              sx(a, 335), sy(a, 305),
              sx(a, 335), sy(a, 420));

    /* bouchon */
    draw_line(layer, color, lw,
              sx(a, 380), sy(a, 290),
              sx(a, 380), sy(a, 205));
    draw_line(layer, color, lw,
              sx(a, 325), sy(a, 205),
              sx(a, 440), sy(a, 205));

    /* bec */
    draw_line(layer, color, lw,
              sx(a, 685), sy(a, 540),
              sx(a, 845), sy(a, 395));
    draw_line(layer, color, lw,
              sx(a, 845), sy(a, 395),
              sx(a, 920), sy(a, 365));

    /* goutte */
    draw_circle_fill(layer, color,
                     sx(a, 875), sy(a, 610),
                     sw(a, 58));
    draw_line(layer, color, lw,
              sx(a, 875), sy(a, 525),
              sx(a, 875), sy(a, 585));
}

/* -------------------- ENGINE -------------------- */

static void draw_engine(lv_layer_t * layer,
                        const lv_area_t * a,
                        lv_color_t color)
{
    const lv_coord_t lw = sw(a, 55);

    /* contour moteur */
    const int pts[][2] = {
        {230, 350}, {320, 350}, {390, 265}, {665, 265},
        {750, 350}, {845, 350}, {845, 665}, {760, 665},
        {705, 730}, {355, 730}, {290, 665}, {230, 665},
        {230, 350}
    };

    for(unsigned i = 0; i + 1 < sizeof(pts)/sizeof(pts[0]); i++) {
        draw_line(layer, color, lw,
                  sx(a, pts[i][0]), sy(a, pts[i][1]),
                  sx(a, pts[i+1][0]), sy(a, pts[i+1][1]));
    }

    /* bouchon supérieur */
    draw_line(layer, color, lw,
              sx(a, 470), sy(a, 265),
              sx(a, 470), sy(a, 175));
    draw_line(layer, color, lw,
              sx(a, 400), sy(a, 175),
              sx(a, 545), sy(a, 175));

    /* connecteur gauche */
    draw_line(layer, color, lw,
              sx(a, 230), sy(a, 430),
              sx(a, 145), sy(a, 430));
    draw_line(layer, color, lw,
              sx(a, 145), sy(a, 350),
              sx(a, 145), sy(a, 575));
    draw_line(layer, color, lw,
              sx(a, 145), sy(a, 575),
              sx(a, 230), sy(a, 575));

    /* connecteur droit */
    draw_line(layer, color, lw,
              sx(a, 845), sy(a, 445),
              sx(a, 915), sy(a, 445));
    draw_line(layer, color, lw,
              sx(a, 915), sy(a, 445),
              sx(a, 915), sy(a, 575));
    draw_line(layer, color, lw,
              sx(a, 915), sy(a, 575),
              sx(a, 845), sy(a, 575));
}

/* -------------------- OBD LINK -------------------- */

static void draw_obd_link(lv_layer_t * layer,
                          const lv_area_t * a,
                          lv_color_t color)
{
    const lv_coord_t lw = sw(a, 65);

    /*
     * Deux maillons horizontaux simplifiés.
     * Le dessin est volontairement géométrique pour rester net à 20-40 px.
     */

    /* maillon gauche */
    draw_line(layer, color, lw,
              sx(a, 200), sy(a, 350),
              sx(a, 420), sy(a, 350));
    draw_line(layer, color, lw,
              sx(a, 200), sy(a, 350),
              sx(a, 130), sy(a, 420));
    draw_line(layer, color, lw,
              sx(a, 130), sy(a, 420),
              sx(a, 130), sy(a, 580));
    draw_line(layer, color, lw,
              sx(a, 130), sy(a, 580),
              sx(a, 200), sy(a, 650));
    draw_line(layer, color, lw,
              sx(a, 200), sy(a, 650),
              sx(a, 430), sy(a, 650));

    /* maillon droit */
    draw_line(layer, color, lw,
              sx(a, 580), sy(a, 350),
              sx(a, 800), sy(a, 350));
    draw_line(layer, color, lw,
              sx(a, 800), sy(a, 350),
              sx(a, 870), sy(a, 420));
    draw_line(layer, color, lw,
              sx(a, 870), sy(a, 420),
              sx(a, 870), sy(a, 580));
    draw_line(layer, color, lw,
              sx(a, 870), sy(a, 580),
              sx(a, 800), sy(a, 650));
    draw_line(layer, color, lw,
              sx(a, 800), sy(a, 650),
              sx(a, 570), sy(a, 650));

    /* liaison centrale */
    draw_line(layer, color, lw,
              sx(a, 350), sy(a, 500),
              sx(a, 650), sy(a, 500));
}

/* -------------------- LVGL WIDGET -------------------- */

static void dash_icon_draw_event(lv_event_t * e)
{
    if(lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t * obj = lv_event_get_target(e);
    dash_icon_data_t * data = lv_obj_get_user_data(obj);
    if(!data) return;

    lv_layer_t * layer = lv_event_get_layer(e);

    lv_area_t a;
    lv_obj_get_coords(obj, &a);

    /* petite marge interne pour éviter de couper les traits */
    lv_coord_t pad = sw(&a, 70);
    a.x1 += pad;
    a.y1 += pad;
    a.x2 -= pad;
    a.y2 -= pad;

    switch(data->type) {
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
        default:
            break;
    }
}

static void dash_icon_delete_event(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    dash_icon_data_t * data = lv_obj_get_user_data(obj);

    if(data) {
        lv_free(data);
        lv_obj_set_user_data(obj, NULL);
    }
}

lv_obj_t * dash_icon_create(lv_obj_t * parent,
                            dash_icon_type_t type,
                            lv_coord_t size,
                            lv_color_t color)
{
    lv_obj_t * obj = lv_obj_create(parent);

    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, size, size);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    dash_icon_data_t * data = lv_malloc(sizeof(dash_icon_data_t));
    if(!data) {
        lv_obj_delete(obj);
        return NULL;
    }

    data->type = type;
    data->color = color;

    lv_obj_set_user_data(obj, data);

    lv_obj_add_event_cb(obj, dash_icon_draw_event,
                        LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(obj, dash_icon_delete_event,
                        LV_EVENT_DELETE, NULL);

    lv_obj_invalidate(obj);
    return obj;
}

void dash_icon_set_color(lv_obj_t * obj, lv_color_t color)
{
    if(!obj) return;

    dash_icon_data_t * data = lv_obj_get_user_data(obj);
    if(!data) return;

    data->color = color;
    lv_obj_invalidate(obj);
}

void dash_icon_set_size(lv_obj_t * obj, lv_coord_t size)
{
    if(!obj) return;
    lv_obj_set_size(obj, size, size);
    lv_obj_invalidate(obj);
}