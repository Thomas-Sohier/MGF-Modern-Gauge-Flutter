#include "dual_arc_dial.h"
#include "gauge_theme.h"

#include <math.h>

// ── État attaché à l'objet jauge ─────────────────────────────────────────────
typedef struct {
    float throttle, throttle_max;
    float primary, primary_max, primary_danger;
} dial_state_t;

// ── Géométrie (miroir de ArcGeometry.dart) ───────────────────────────────────

static inline float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static inline float progress(float value, float max_value) {
    if (max_value <= 0.0f) return 0.0f;
    return clamp01(value / max_value);
}

// Écart angulaire d'un segment (degrés), gaps inclus.
static inline float segment_deg(void) {
    const float gap = MGF_PRIMARY_SEG_SPACING;
    return (MGF_SWEEP_DEG - gap * (MGF_PRIMARY_SEGMENTS - 1)) / MGF_PRIMARY_SEGMENTS;
}

// Angle de début du segment `i` (degrés, convention LVGL).
static inline float segment_start(int i) {
    return MGF_START_ANGLE_DEG + i * (segment_deg() + MGF_PRIMARY_SEG_SPACING);
}

// Premier segment situé au niveau ou au-delà du seuil de danger.
static int danger_segment_start(float danger, float max_value) {
    if (danger <= 0.0f || max_value <= 0.0f) return MGF_PRIMARY_SEGMENTS + 1;
    return (int)floorf(progress(danger, max_value) * MGF_PRIMARY_SEGMENTS);
}

// Résolution visuelle : 0,5 % de la plage normalisée (cf. gaugeValueEpsilon).
#define MGF_VALUE_EPSILON 0.005f

static bool gauge_value_changed(float prev, float next, float max_value, float danger) {
    if (fabsf(progress(prev, max_value) - progress(next, max_value)) > MGF_VALUE_EPSILON) {
        return true;
    }
    if (danger <= 0.0f) return false;
    return (prev >= danger) != (next >= danger); // transition de seuil jamais filtrée
}

// ── Rendu ─────────────────────────────────────────────────────────────────────

static void draw_arc(lv_layer_t *layer, int32_t cx, int32_t cy, int32_t radius,
                     int32_t width, float a0, float a1, lv_color_t color, lv_opa_t opa) {
    lv_draw_arc_dsc_t dsc;
    lv_draw_arc_dsc_init(&dsc);
    dsc.center.x = cx;
    dsc.center.y = cy;
    dsc.radius = radius;
    dsc.width = width;
    dsc.start_angle = a0;
    dsc.end_angle = a1;
    dsc.color = color;
    dsc.opa = opa;
    dsc.rounded = 0;
    lv_draw_arc(layer, &dsc);
}

static void dial_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    dial_state_t *st = lv_obj_get_user_data(obj);

    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int32_t w = lv_area_get_width(&a);
    const int32_t h = lv_area_get_height(&a);
    const int32_t cx = a.x1 + w / 2;
    const int32_t cy = a.y1 + h / 2;
    const float base_radius = (float)LV_MIN(w, h) / 2.0f;

    const int32_t seg_h = (int32_t)MGF_PRIMARY_SEG_HEIGHT;
    const int32_t primary_radius = (int32_t)(base_radius - MGF_PRIMARY_SEG_HEIGHT + 10.0f);
    const int32_t throttle_radius =
        (int32_t)((base_radius - MGF_PRIMARY_SEG_HEIGHT) * MGF_THROTTLE_RADIUS_FACT);
    const int32_t throttle_h = (int32_t)MGF_THROTTLE_SEG_HEIGHT;
    const float seg_deg = segment_deg();
    const int danger_start = danger_segment_start(st->primary_danger, st->primary_max);

    // 1) Fond de l'arc primaire : 20 segments éteints (danger en rouge atténué).
    for (int i = 0; i < MGF_PRIMARY_SEGMENTS; i++) {
        const float s = segment_start(i);
        if (i >= danger_start) {
            draw_arc(layer, cx, cy, primary_radius, seg_h, s, s + seg_deg,
                     MGF_COL_DANGER_INACTIVE, MGF_OPA_DANGER_INACTIVE);
        } else {
            draw_arc(layer, cx, cy, primary_radius, seg_h, s, s + seg_deg,
                     MGF_COL_INACTIVE, LV_OPA_COVER);
        }
    }

    // 2) Fond de l'arc papillon (intérieur, 1 segment continu).
    draw_arc(layer, cx, cy, throttle_radius, throttle_h,
             MGF_START_ANGLE_DEG, MGF_START_ANGLE_DEG + MGF_SWEEP_DEG,
             MGF_COL_INACTIVE, LV_OPA_COVER);

    // 3) Remplissage actif de l'arc primaire.
    const float prim_progress = progress(st->primary, st->primary_max);
    const float continuous = prim_progress * MGF_PRIMARY_SEGMENTS;
    const int full = (int)floorf(continuous);
    const float partial = continuous - (float)full;

    for (int i = 0; i <= full && i < MGF_PRIMARY_SEGMENTS; i++) {
        const float s = segment_start(i);
        const lv_color_t col = (i >= danger_start) ? MGF_COL_DANGER : MGF_COL_ACTIVE;
        if (i < full) {
            draw_arc(layer, cx, cy, primary_radius, seg_h, s, s + seg_deg, col, LV_OPA_COVER);
        } else if (partial > 0.0f) {
            draw_arc(layer, cx, cy, primary_radius, seg_h, s, s + seg_deg * partial,
                     col, LV_OPA_COVER);
        }
    }

    // 4) Remplissage actif de l'arc papillon.
    const float thr_progress = progress(st->throttle, st->throttle_max);
    if (thr_progress > 0.0f) {
        draw_arc(layer, cx, cy, throttle_radius, throttle_h,
                 MGF_START_ANGLE_DEG, MGF_START_ANGLE_DEG + MGF_SWEEP_DEG * thr_progress,
                 MGF_COL_ACTIVE, LV_OPA_COVER);
    }
}

// ── API ───────────────────────────────────────────────────────────────────────

lv_obj_t *dual_arc_dial_create(lv_obj_t *parent) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    dial_state_t *st = lv_malloc(sizeof(dial_state_t));
    *st = (dial_state_t){
        .throttle = 0, .throttle_max = 100,
        .primary = 0, .primary_max = 8500, .primary_danger = 7000,
    };
    lv_obj_set_user_data(obj, st);
    lv_obj_add_event_cb(obj, dial_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}

void dual_arc_dial_set_values(lv_obj_t *dial,
                              float throttle, float throttle_max,
                              float primary, float primary_max,
                              float primary_danger) {
    dial_state_t *st = lv_obj_get_user_data(dial);

    const bool changed =
        gauge_value_changed(st->primary, primary, primary_max, primary_danger) ||
        gauge_value_changed(st->throttle, throttle, throttle_max, -1.0f) ||
        st->primary_max != primary_max || st->primary_danger != primary_danger;

    st->throttle = throttle;
    st->throttle_max = throttle_max;
    st->primary = primary;
    st->primary_max = primary_max;
    st->primary_danger = primary_danger;

    if (changed) lv_obj_invalidate(dial);
}
