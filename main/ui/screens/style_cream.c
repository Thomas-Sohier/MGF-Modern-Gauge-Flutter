#include "ui/screens/style_cream.h"
#include "ui/icons/gauge_icons.h"

#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── Palette crème ─────────────────────────────────────────────────────────────
#define CREAM_BG     lv_color_hex(0xE9E5D8)
#define CREAM_DOME   lv_color_hex(0xF3F0E7) // disque intérieur (effet bombé)
#define CREAM_TEXT   lv_color_hex(0x1E1E1E)
#define CREAM_DIM    lv_color_hex(0x6B675E)
#define CREAM_TICK   lv_color_hex(0x4A4844)
#define CREAM_RED    lv_color_hex(0xC0201A)
#define CREAM_LINE   lv_color_hex(0x9C978B)

// ── Échelle en graduations ────────────────────────────────────────────────────
#define C_TICKS     41
#define C_START_DEG 180.0f
#define C_SWEEP_DEG 180.0f

#define RPM_MAX    8500.0f
#define RPM_DANGER 7000.0f

enum { M_LDR = 0, M_BATT, M_COUNT }; // la crème n'expose que 2 cellules (cf. photo)
static const char *const kNames[M_COUNT] = {"LDR", "BATT"};
static const mgf_icon_t kIcons[M_COUNT] = {MGF_ICON_COOLANT, MGF_ICON_BATTERY};

struct cream_screen_s {
    lv_obj_t *arc;
    lv_obj_t *value;
    lv_obj_t *value_sub;
    lv_obj_t *ind_value[M_COUNT];
    lv_obj_t *ind_icon[M_COUNT];
};

static void arc_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int32_t w = lv_area_get_width(&a), h = lv_area_get_height(&a);
    const float cx = a.x1 + w / 2.0f, cy = a.y1 + h / 2.0f;
    const float base = (float)LV_MIN(w, h) / 2.0f;
    const float r_out = base - 8.0f;
    const float r_in = r_out - base * 0.085f;

    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.opa = LV_OPA_COVER;
    d.round_start = d.round_end = 1;

    for (int i = 0; i < C_TICKS; i++) {
        const float frac = (float)i / (C_TICKS - 1);
        const float ang = (C_START_DEG + C_SWEEP_DEG * frac) * (float)M_PI / 180.0f;
        const float ca = cosf(ang), sa = sinf(ang);
        const float tick_val = frac * RPM_MAX;

        if (tick_val >= RPM_DANGER) {
            d.color = CREAM_RED; d.width = 5;
        } else if (i < 3) {
            d.color = CREAM_TEXT; d.width = 6; // amorce d'échelle marquée
        } else {
            d.color = CREAM_TICK; d.width = 4;
        }
        d.p1.x = cx + r_out * ca; d.p1.y = cy + r_out * sa;
        d.p2.x = cx + r_in * ca;  d.p2.y = cy + r_in * sa;
        lv_draw_line(layer, &d);
    }
}

cream_screen_t *cream_screen_create(lv_obj_t *parent) {
    cream_screen_t *scr = lv_malloc(sizeof(cream_screen_t));
    lv_memzero(scr, sizeof(*scr));

    lv_obj_set_style_bg_color(parent, CREAM_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    const int32_t w = lv_obj_get_width(parent);
    const int32_t h = lv_obj_get_height(parent);
    const int32_t cx = w / 2, cy = h / 2;
    const float base = (float)LV_MIN(w, h) / 2.0f;

    // Disque intérieur plus clair (effet cadran bombé).
    lv_obj_t *dome = lv_obj_create(parent);
    lv_obj_remove_style_all(dome);
    const int32_t dd = (int32_t)(base * 1.86f);
    lv_obj_set_size(dome, dd, dd);
    lv_obj_align(dome, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(dome, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dome, CREAM_DOME, 0);
    lv_obj_set_style_bg_opa(dome, LV_OPA_COVER, 0);
    lv_obj_clear_flag(dome, LV_OBJ_FLAG_SCROLLABLE);

    scr->arc = lv_obj_create(parent);
    lv_obj_remove_style_all(scr->arc);
    lv_obj_set_size(scr->arc, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(scr->arc, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr->arc, arc_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    scr->value = lv_label_create(parent);
    lv_obj_set_style_text_font(scr->value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(scr->value, CREAM_TEXT, 0);
    lv_label_set_text(scr->value, "0");
    lv_obj_align(scr->value, LV_ALIGN_CENTER, 0, -18);

    scr->value_sub = lv_label_create(parent);
    lv_obj_set_style_text_font(scr->value_sub, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(scr->value_sub, CREAM_DIM, 0);
    lv_label_set_text(scr->value_sub, "RPM");
    lv_obj_align(scr->value_sub, LV_ALIGN_CENTER, 0, 20);

    // Filet séparateur.
    lv_obj_t *divider = lv_obj_create(parent);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, (int32_t)(base * 0.95f), 2);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, 52);
    lv_obj_set_style_bg_color(divider, CREAM_LINE, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

    // Deux cellules icône + valeur, alignées sous le filet.
    const int32_t cell_w = 150;
    const int32_t offs[M_COUNT] = {-90, 90};
    for (int i = 0; i < M_COUNT; i++) {
        lv_obj_t *cell = lv_obj_create(parent);
        lv_obj_remove_style_all(cell);
        lv_obj_set_size(cell, cell_w, 70);
        lv_obj_align(cell, LV_ALIGN_CENTER, offs[i], 108);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(cell, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cell, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(cell, 8, 0);

        scr->ind_icon[i] = mgf_icon_create(cell, kIcons[i], 30);
        mgf_icon_set_color(scr->ind_icon[i], CREAM_TEXT);

        scr->ind_value[i] = lv_label_create(cell);
        lv_obj_set_style_text_font(scr->ind_value[i], &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(scr->ind_value[i], CREAM_TEXT, 0);
        lv_label_set_text(scr->ind_value[i], "");
    }

    // Séparateur vertical entre les deux cellules.
    lv_obj_t *vsep = lv_obj_create(parent);
    lv_obj_remove_style_all(vsep);
    lv_obj_set_size(vsep, 2, 56);
    lv_obj_align(vsep, LV_ALIGN_CENTER, 0, 108);
    lv_obj_set_style_bg_color(vsep, CREAM_LINE, 0);
    lv_obj_set_style_bg_opa(vsep, LV_OPA_COVER, 0);

    return scr;
}

void cream_screen_update(cream_screen_t *scr, const ecu_data_t *d) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%.0f", d->rpm);
    lv_label_set_text(scr->value, buf);
    lv_obj_set_style_text_color(scr->value,
                                d->rpm >= RPM_DANGER ? CREAM_RED : CREAM_TEXT, 0);

    // NB : la police Montserrat LVGL par défaut n'inclut pas « ° », on écrit « C ».
    snprintf(buf, sizeof(buf), "%.0f C", d->coolant_temp);
    lv_label_set_text(scr->ind_value[M_LDR], buf);
    snprintf(buf, sizeof(buf), "%.1fV", d->battery_voltage);
    lv_label_set_text(scr->ind_value[M_BATT], buf);
}
