#include "ui/icons/gauge_icons.h"

#include <math.h>

typedef struct {
    mgf_icon_t id;
    lv_color_t color;
} icon_state_t;

// ── Primitives ────────────────────────────────────────────────────────────────

static void fill_rect(lv_layer_t *l, float x1, float y1, float x2, float y2,
                      float radius, lv_color_t c) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = c;
    d.bg_opa = LV_OPA_COVER;
    d.border_width = 0;
    d.radius = (int32_t)radius;
    lv_area_t a = {(int32_t)x1, (int32_t)y1, (int32_t)x2, (int32_t)y2};
    lv_draw_rect(l, &d, &a);
}

static void stroke_rect(lv_layer_t *l, float x1, float y1, float x2, float y2,
                        float radius, float width, lv_color_t c) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.border_color = c;
    d.border_opa = LV_OPA_COVER;
    d.border_width = (int32_t)width;
    d.radius = (int32_t)radius;
    lv_area_t a = {(int32_t)x1, (int32_t)y1, (int32_t)x2, (int32_t)y2};
    lv_draw_rect(l, &d, &a);
}

static void line(lv_layer_t *l, float x1, float y1, float x2, float y2,
                 float width, lv_color_t c) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = c;
    d.opa = LV_OPA_COVER;
    d.width = (int32_t)width;
    d.round_start = d.round_end = 1;
    d.p1.x = x1; d.p1.y = y1;
    d.p2.x = x2; d.p2.y = y2;
    lv_draw_line(l, &d);
}

static void circle(lv_layer_t *l, float cx, float cy, float r, lv_color_t c) {
    fill_rect(l, cx - r, cy - r, cx + r, cy + r, r, c);
}

// ── Dessin de chaque icône (repère centré, côté s) ───────────────────────────

static void draw_icon(lv_layer_t *l, mgf_icon_t id, float cx, float cy, float s,
                      lv_color_t c) {
    const float lw = LV_MAX(2.0f, s * 0.08f); // épaisseur de trait
    switch (id) {
        case MGF_ICON_COOLANT: // thermomètre : tige + bulbe
            fill_rect(l, cx - 0.09f * s, cy - 0.38f * s, cx + 0.09f * s, cy + 0.16f * s,
                      0.09f * s, c);
            circle(l, cx, cy + 0.26f * s, 0.19f * s, c);
            break;

        case MGF_ICON_OIL: // burette : corps + bec + goutte
            fill_rect(l, cx - 0.34f * s, cy - 0.02f * s, cx + 0.20f * s, cy + 0.26f * s,
                      0.06f * s, c);
            line(l, cx + 0.16f * s, cy - 0.01f * s, cx + 0.42f * s, cy - 0.22f * s, lw, c);
            circle(l, cx + 0.44f * s, cy - 0.28f * s, 0.07f * s, c);
            break;

        case MGF_ICON_BATTERY: // corps + bornes + barres
            stroke_rect(l, cx - 0.36f * s, cy - 0.16f * s, cx + 0.36f * s, cy + 0.22f * s,
                        0.05f * s, lw, c);
            fill_rect(l, cx - 0.22f * s, cy - 0.26f * s, cx - 0.08f * s, cy - 0.16f * s, 0, c);
            fill_rect(l, cx + 0.08f * s, cy - 0.26f * s, cx + 0.22f * s, cy - 0.16f * s, 0, c);
            fill_rect(l, cx - 0.14f * s, cy - 0.05f * s, cx - 0.06f * s, cy + 0.13f * s, 0, c);
            fill_rect(l, cx + 0.06f * s, cy - 0.05f * s, cx + 0.14f * s, cy + 0.13f * s, 0, c);
            break;

        case MGF_ICON_OBD: // deux maillons de chaîne
            stroke_rect(l, cx - 0.36f * s, cy - 0.16f * s, cx + 0.02f * s, cy + 0.16f * s,
                        0.16f * s, lw, c);
            stroke_rect(l, cx - 0.02f * s, cy - 0.16f * s, cx + 0.36f * s, cy + 0.16f * s,
                        0.16f * s, lw, c);
            break;

        case MGF_ICON_TACH: { // compte-tours : cadran + aiguille
            lv_draw_arc_dsc_t d;
            lv_draw_arc_dsc_init(&d);
            d.center.x = (int32_t)cx;
            d.center.y = (int32_t)cy;
            d.radius = (int32_t)(0.30f * s);
            d.width = (int32_t)lw;
            d.start_angle = 0;
            d.end_angle = 360;
            d.color = c;
            d.opa = LV_OPA_COVER;
            lv_draw_arc(l, &d);
            line(l, cx, cy, cx + 0.22f * s * cosf(-0.9f), cy + 0.22f * s * sinf(-0.9f), lw, c);
            break;
        }
        default:
            break;
    }
}

// ── Widget ────────────────────────────────────────────────────────────────────

static void icon_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    icon_state_t *st = lv_obj_get_user_data(obj);

    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const float cx = (a.x1 + a.x2) / 2.0f;
    const float cy = (a.y1 + a.y2) / 2.0f;
    const float s = (float)LV_MIN(lv_area_get_width(&a), lv_area_get_height(&a));
    draw_icon(layer, st->id, cx, cy, s, st->color);
}

lv_obj_t *mgf_icon_create(lv_obj_t *parent, mgf_icon_t id, int32_t size) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, size, size);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    icon_state_t *st = lv_malloc(sizeof(icon_state_t));
    st->id = id;
    st->color = lv_color_white();
    lv_obj_set_user_data(obj, st);
    lv_obj_add_event_cb(obj, icon_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}

void mgf_icon_set_color(lv_obj_t *icon, lv_color_t color) {
    icon_state_t *st = lv_obj_get_user_data(icon);
    if (st->color.red == color.red && st->color.green == color.green &&
        st->color.blue == color.blue) {
        return;
    }
    st->color = color;
    lv_obj_invalidate(icon);
}
