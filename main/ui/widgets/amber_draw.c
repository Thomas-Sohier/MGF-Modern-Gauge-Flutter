#include "ui/widgets/amber_draw.h"

#include <math.h>

#include "src/misc/lv_area_private.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int32_t scaled(float value) {
    return (int32_t)lroundf(value);
}

ui_layout_t amber_draw_layout(const lv_area_t *area) {
    ui_layout_t layout =
        ui_layout_fit(lv_area_get_width(area), lv_area_get_height(area));
    layout.ox += area->x1;
    layout.oy += area->y1;
    return layout;
}

float amber_clampf(float value, float low, float high) {
    if (value < low) return low;
    if (value > high) return high;
    return value;
}

float amber_progress(float value, float low, float high) {
    if (high <= low) return 0.0f;
    return (amber_clampf(value, low, high) - low) / (high - low);
}

void amber_draw_line(lv_layer_t *layer, const ui_layout_t *layout, float x1,
                     float y1, float x2, float y2, float width,
                     lv_color_t color, bool rounded) {
    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.p1.x = scaled(ui_layout_x(layout, x1));
    dsc.p1.y = scaled(ui_layout_y(layout, y1));
    dsc.p2.x = scaled(ui_layout_x(layout, x2));
    dsc.p2.y = scaled(ui_layout_y(layout, y2));
    dsc.width = LV_MAX(1, scaled(width * layout->scale));
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.round_start = rounded;
    dsc.round_end = rounded;
    lv_draw_line(layer, &dsc);
}

void amber_draw_arc(lv_layer_t *layer, const ui_layout_t *layout, float cx,
                    float cy, float radius, float width, float start, float end,
                    lv_color_t color, bool rounded) {
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.center.x = scaled(ui_layout_x(layout, cx));
    dsc.center.y = scaled(ui_layout_y(layout, cy));
    dsc.radius = LV_MAX(1, scaled(radius * layout->scale));
    dsc.width = LV_MAX(1, scaled(width * layout->scale));
    dsc.start_angle = start;
    dsc.end_angle = end;
    dsc.color = color;
    dsc.opa = LV_OPA_COVER;
    dsc.rounded = rounded;
    amber_draw_arc_clipped(layer, &dsc);
}

void amber_draw_arc_clipped(lv_layer_t *layer, const lv_draw_arc_dsc_t *dsc) {
    // Secteur encadrant au degré entier (LVGL tronque les angles), comme
    // amber_invalidate_sector. Un arc complet ou replié garde le chemin LVGL.
    const float start = floorf(dsc->start_angle);
    const float end = ceilf(dsc->end_angle);
    if (end <= start || end - start >= 360.0f) {
        lv_draw_arc(layer, dsc);
        return;
    }
    const float r_out = (float)dsc->radius;
    const float r_in = (float)LV_MAX(0, dsc->radius - dsc->width);
    float min_x = INFINITY, max_x = -INFINITY;
    float min_y = INFINITY, max_y = -INFINITY;
#define SECTOR_ADD(angle, radius)                                              \
    do {                                                                       \
        const float rad = (angle) * (float)M_PI / 180.0f;                      \
        const float px = cosf(rad) * (radius);                                 \
        const float py = sinf(rad) * (radius);                                 \
        min_x = fminf(min_x, px);                                              \
        max_x = fmaxf(max_x, px);                                              \
        min_y = fminf(min_y, py);                                              \
        max_y = fmaxf(max_y, py);                                              \
    } while (0)
    SECTOR_ADD(start, r_in);
    SECTOR_ADD(start, r_out);
    SECTOR_ADD(end, r_in);
    SECTOR_ADD(end, r_out);
    for (int k = (int)ceilf(start / 90.0f); k * 90.0f <= end; k++) {
        SECTOR_ADD(k * 90.0f, r_out);
    }
#undef SECTOR_ADD
    // Marge : antialiasing, plus le demi-disque des extrémités arrondies.
    const float margin = 2.0f + (dsc->rounded ? (float)dsc->width : 0.0f);
    const lv_area_t sector = {
        .x1 = dsc->center.x + (int32_t)floorf(min_x - margin),
        .y1 = dsc->center.y + (int32_t)floorf(min_y - margin),
        .x2 = dsc->center.x + (int32_t)ceilf(max_x + margin),
        .y2 = dsc->center.y + (int32_t)ceilf(max_y + margin),
    };
    const lv_area_t saved = layer->_clip_area;
    lv_area_t clip;
    if (!lv_area_intersect(&clip, &saved, &sector)) return;
    // Zone entièrement à l'intérieur du cercle intérieur : l'anneau ne peut
    // pas la toucher (cas d'un texte central redessiné sous un long filet).
    const float far_x = (float)LV_MAX(LV_ABS(clip.x1 - dsc->center.x),
                                      LV_ABS(clip.x2 - dsc->center.x));
    const float far_y = (float)LV_MAX(LV_ABS(clip.y1 - dsc->center.y),
                                      LV_ABS(clip.y2 - dsc->center.y));
    const float reach = r_in - margin;
    if (reach > 0.0f && far_x * far_x + far_y * far_y < reach * reach) return;
    layer->_clip_area = clip;
    lv_draw_arc(layer, dsc);
    layer->_clip_area = saved;
}

void amber_draw_arc_wrapped(lv_layer_t *layer, const ui_layout_t *layout,
                            float cx, float cy, float radius, float width,
                            float start, float sweep, lv_color_t color,
                            bool rounded) {
    float normalized = fmodf(start, 360.0f);
    if (normalized < 0.0f) normalized += 360.0f;
    if (sweep >= 360.0f) {
        amber_draw_arc(layer, layout, cx, cy, radius, width, 0.0f, 360.0f,
                       color, rounded);
        return;
    }
    const float end = normalized + amber_clampf(sweep, 0.0f, 360.0f);
    if (end <= 360.0f) {
        amber_draw_arc(layer, layout, cx, cy, radius, width, normalized, end,
                       color, rounded);
    } else {
        // Un morceau de longueur (quasi) nulle serait interprété par LVGL
        // comme un cercle complet (angles entiers égaux) : on l'omet.
        if (360.0f - normalized >= 1.0f) {
            amber_draw_arc(layer, layout, cx, cy, radius, width, normalized,
                           360.0f, color, rounded);
        }
        if (end - 360.0f >= 1.0f) {
            amber_draw_arc(layer, layout, cx, cy, radius, width, 0.0f,
                           end - 360.0f, color, rounded);
        }
    }
}

void amber_draw_circle(lv_layer_t *layer, const ui_layout_t *layout, float cx,
                       float cy, float radius, float width, lv_color_t color) {
    amber_draw_arc(layer, layout, cx, cy, radius, width, 0.0f, 360.0f, color,
                   false);
}

void amber_draw_dot(lv_layer_t *layer, const ui_layout_t *layout, float x,
                    float y, float radius, lv_color_t color) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = LV_RADIUS_CIRCLE;
    const int32_t r = LV_MAX(1, scaled(radius * layout->scale));
    lv_area_t area = {
        .x1 = scaled(ui_layout_x(layout, x)) - r,
        .y1 = scaled(ui_layout_y(layout, y)) - r,
        .x2 = scaled(ui_layout_x(layout, x)) + r,
        .y2 = scaled(ui_layout_y(layout, y)) + r,
    };
    lv_draw_rect(layer, &dsc, &area);
}

void amber_draw_tick(lv_layer_t *layer, const ui_layout_t *layout, float cx,
                     float cy, float angle_deg, float inner, float outer,
                     float width, lv_color_t color, bool rounded) {
    const float radians = angle_deg * (float)M_PI / 180.0f;
    const float cosine = cosf(radians);
    const float sine = sinf(radians);
    amber_draw_line(layer, layout, cx + cosine * inner, cy + sine * inner,
                    cx + cosine * outer, cy + sine * outer, width, color,
                    rounded);
}

void amber_invalidate_sector(lv_obj_t *obj, float cx, float cy, float r_in,
                             float r_out, float start, float sweep,
                             float margin_px) {
    if (obj == NULL || sweep <= 0.0f) return;
    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    const ui_layout_t layout = amber_draw_layout(&coords);
    // Le rendu SW de LVGL 9.2 tronque les angles d'arc au degré entier :
    // le bord réel peut être décalé de près d'1° (≈4 px à r=237). On élargit
    // donc le secteur au degré entier encadrant, plus 1° de sécurité.
    float end = ceilf(start + sweep) + 1.0f;
    start = floorf(start) - 1.0f;
    if (end - start >= 360.0f) {
        start = 0.0f;
        end = 360.0f;
    }
    float min_x = INFINITY, max_x = -INFINITY;
    float min_y = INFINITY, max_y = -INFINITY;
#define SECTOR_ADD(angle, radius)                                              \
    do {                                                                       \
        const float rad = (angle) * (float)M_PI / 180.0f;                      \
        const float px = cx + cosf(rad) * (radius);                            \
        const float py = cy + sinf(rad) * (radius);                            \
        min_x = fminf(min_x, px);                                              \
        max_x = fmaxf(max_x, px);                                              \
        min_y = fminf(min_y, py);                                              \
        max_y = fmaxf(max_y, py);                                              \
    } while (0)
    SECTOR_ADD(start, r_in);
    SECTOR_ADD(start, r_out);
    SECTOR_ADD(end, r_in);
    SECTOR_ADD(end, r_out);
    // Extrêmes du cercle extérieur atteints aux angles cardinaux inclus.
    for (int k = (int)ceilf(start / 90.0f); k * 90.0f <= end; k++) {
        SECTOR_ADD(k * 90.0f, r_out);
    }
#undef SECTOR_ADD
    lv_area_t area = {
        .x1 = (int32_t)floorf(ui_layout_x(&layout, min_x) - margin_px),
        .y1 = (int32_t)floorf(ui_layout_y(&layout, min_y) - margin_px),
        .x2 = (int32_t)ceilf(ui_layout_x(&layout, max_x) + margin_px),
        .y2 = (int32_t)ceilf(ui_layout_y(&layout, max_y) + margin_px),
    };
    lv_obj_invalidate_area(obj, &area);
}
