#include "rpm_screen.h"
#include "dual_arc_dial.h"
#include "gauge_theme.h"

#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Métriques (même ordre que rpm_screen.dart) ───────────────────────────────
enum { M_OBD = 0, M_RPM, M_LDR, M_BATT, M_HUILE, M_COUNT };
#define PRIMARY_INDEX M_RPM

// Métrique primaire (jauge) : RPM.
#define RPM_MAX     8500.0f
#define RPM_DANGER  7000.0f

static const char *const kNames[M_COUNT]   = {"OBD", "RPM", "LDR", "BATT", "HUILE"};
static const char *const kUnits[M_COUNT]   = {"", "", " C", " V", " C"};
static const float        kDanger[M_COUNT] = {0.0f, 7000.0f, 150.0f, 16.0f, 150.0f};

struct rpm_screen_s {
    lv_obj_t *dial;
    lv_obj_t *value_label; // grand nombre central
    lv_obj_t *value_sub;   // libellé "RPM"
    lv_obj_t *ind_value[M_COUNT];
    lv_obj_t *ind_name[M_COUNT];
};

static float metric_value(int m, const ecu_data_t *d) {
    switch (m) {
        case M_RPM:   return d->rpm;
        case M_LDR:   return d->coolant_temp;
        case M_BATT:  return d->battery_voltage;
        case M_HUILE: return d->oil_temp;
        default:      return 0.0f;
    }
}

// ── Construction ──────────────────────────────────────────────────────────────

static lv_obj_t *make_indicator_label(lv_obj_t *parent, const lv_font_t *font) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, MGF_COL_ON_SURFACE_DIM, 0);
    lv_label_set_text(l, "");
    return l;
}

rpm_screen_t *rpm_screen_create(lv_obj_t *parent) {
    rpm_screen_t *scr = lv_malloc(sizeof(rpm_screen_t));
    lv_memzero(scr, sizeof(*scr));

    const int32_t w = lv_obj_get_width(parent);
    const int32_t h = lv_obj_get_height(parent);
    const int32_t cx = w / 2;
    const int32_t cy = h / 2;
    const float base_radius = (float)LV_MIN(w, h) / 2.0f;

    // Disque de fond de la jauge (GaugeThemeBackground : cercle plein + bordure).
    lv_obj_t *disc = lv_obj_create(parent);
    lv_obj_remove_style_all(disc);
    const int32_t d = (int32_t)(base_radius * 2.0f);
    lv_obj_set_size(disc, d, d);
    lv_obj_align(disc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, MGF_COL_GAUGE_BG, 0);
    lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(disc, MGF_COL_BG, 0);
    lv_obj_set_style_border_width(disc, 2, 0);
    lv_obj_clear_flag(disc, LV_OBJ_FLAG_SCROLLABLE);

    // Jauge double arc (remplit l'écran, dessinée par-dessus le disque).
    scr->dial = dual_arc_dial_create(parent);

    // Contenu central : grande valeur + libellé.
    scr->value_label = lv_label_create(parent);
    lv_obj_set_style_text_font(scr->value_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(scr->value_label, MGF_COL_ACTIVE, 0);
    lv_label_set_text(scr->value_label, "0");
    lv_obj_align(scr->value_label, LV_ALIGN_CENTER, 0, -12);

    scr->value_sub = lv_label_create(parent);
    lv_obj_set_style_text_font(scr->value_sub, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(scr->value_sub, MGF_COL_ACTIVE, 0);
    lv_label_set_text(scr->value_sub, "RPM");
    lv_obj_align(scr->value_sub, LV_ALIGN_CENTER, 0, 34);

    // Indicateurs de métriques disposés en arc sous le centre.
    // (miroir de _ArcFlowDelegate : rayon 0.75, arc de π*0.15 à π*0.85, y bas)
    const float radius = base_radius * 0.75f;
    const float start_angle = (float)M_PI * 0.15f;
    const float sweep_angle = (float)M_PI * 0.7f;
    for (int i = 0; i < M_COUNT; i++) {
        const float angle = start_angle + (float)i * (sweep_angle / (M_COUNT - 1));
        const int32_t ix = cx + (int32_t)(radius * cosf(angle));
        const int32_t iy = cy + (int32_t)(radius * sinf(angle));

        lv_obj_t *cell = lv_obj_create(parent);
        lv_obj_remove_style_all(cell);
        lv_obj_set_size(cell, 120, 74);
        lv_obj_set_pos(cell, ix - 60, iy - 37);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        scr->ind_value[i] = make_indicator_label(cell, &lv_font_montserrat_28);
        scr->ind_name[i] = make_indicator_label(cell, &lv_font_montserrat_14);
        lv_label_set_text(scr->ind_name[i], kNames[i]);
    }

    return scr;
}

// ── Rafraîchissement ────────────────────────────────────────────────────────

void rpm_screen_update(rpm_screen_t *scr, const ecu_data_t *d) {
    // Jauge : RPM (primaire) + papillon (arc intérieur).
    dual_arc_dial_set_values(scr->dial, d->throttle, 100.0f,
                             d->rpm, RPM_MAX, RPM_DANGER);

    // Valeur centrale.
    char buf[24];
    snprintf(buf, sizeof(buf), "%.0f", d->rpm);
    lv_label_set_text(scr->value_label, buf);
    const lv_color_t primary_col =
        (d->rpm >= RPM_DANGER) ? MGF_COL_DANGER : MGF_COL_ACTIVE;
    lv_obj_set_style_text_color(scr->value_label, primary_col, 0);
    lv_obj_set_style_text_color(scr->value_sub, primary_col, 0);

    // Indicateurs.
    for (int i = 0; i < M_COUNT; i++) {
        lv_color_t col;
        if (i == M_OBD) {
            lv_label_set_text(scr->ind_value[i],
                              d->connected ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE);
            col = d->connected ? MGF_COL_ACTIVE : MGF_COL_ON_SURFACE_DIM;
        } else {
            const float v = metric_value(i, d);
            const bool danger = (kDanger[i] > 0.0f) && (v >= kDanger[i]);
            if (i == M_BATT) {
                snprintf(buf, sizeof(buf), "%.2f%s", v, kUnits[i]);
            } else {
                snprintf(buf, sizeof(buf), "%.0f%s", v, kUnits[i]);
            }
            lv_label_set_text(scr->ind_value[i], buf);
            col = danger ? MGF_COL_DANGER
                         : (i == PRIMARY_INDEX ? MGF_COL_ACTIVE : MGF_COL_ON_SURFACE_DIM);
        }
        lv_obj_set_style_text_color(scr->ind_value[i], col, 0);
        lv_obj_set_style_text_color(scr->ind_name[i], col, 0);
    }
}
