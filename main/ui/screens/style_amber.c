#include "ui/screens/style_amber.h"
#include "ui/icons/dash_icons.h"
#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_ui.h"
#include "ui/widgets/amber_value.h"
#include "ui/ui_layout.h"
#include "domain/value_smoothing.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

// ── 1. CONSTANTES : géométrie (repère partagé 320 px) ───────────────────────

// Compte-tours
#define DIAL_CX       (UI_REFERENCE_SIZE * 0.5f)
#define DIAL_CY       DIAL_CX
#define DIAL_R_OUT    (DIAL_CX - UI_EDGE_MARGIN)
#define DIAL_R_IN     (DIAL_R_OUT - 31.0f)

#define SEG_COUNT     26       // 26 barres
#define SEG_GAP_DEG   1.80f    // espacement angulaire constant entre chaque barre

#define DIAL_START    180.0f   // 9h (horizontale gauche)
#define DIAL_SWEEP    180.0f   // demi-cercle jusqu'à 3h (horizontale droite)

// Plage régime
#define RPM_MIN       0.0f
#define RPM_MAX       8000.0f
// Constante de temps du lissage d'affichage (trames MEMS toutes ~200 ms).
#define RPM_SMOOTHING_TAU_MS 100.0f

// Traits de séparation
#define SEP_W         1.333f
#define SEP_GAP       3.5f

// Ligne horizontale
#define HSEP_Y        190.0f
#define HSEP_X1       6.667f
#define HSEP_X2       313.333f

// Séparateurs verticaux
#define VSEP_TOP_OUT  168.0f
#define VSEP_TOP_MID  (HSEP_Y + SEP_GAP)
static const float kVSep[3][2] = {
    {83.333f, VSEP_TOP_OUT},
    {160.0f, VSEP_TOP_MID},
    {236.667f, VSEP_TOP_OUT},
};
#define VSEP_REF_X       83.333f
#define VSEP_REF_BOTTOM  288.0f

// Valeur centrale
#define RPM_VAL_X     162.0f
#define RPM_VAL_Y     125.333f
#define RPM_UNIT_Y    170.0f

// ── 1. CONSTANTES : indicateurs bas ──────────────────────────────────────────
enum { M_COOLANT = 0, M_BATTERY, M_OIL, M_OBD, M_COUNT };
typedef struct {
    dash_icon_type_t icon;
    float x, icon_y, label_y;
} ind_def_t;

static const ind_def_t kInd[M_COUNT] = {
    {DASH_ICON_COOLANT,  45.0f,  190.333f, 225.333f},
    {DASH_ICON_BATTERY, 121.667f, 220.333f, 255.333f},
    {DASH_ICON_OIL,     198.333f, 220.333f, 255.333f},
    {DASH_ICON_OBD_LINK,275.0f,  190.333f, 225.333f},
};
#define IND_ICON_SZ   48.0f

// ── État / mise à l'échelle ──────────────────────────────────────────────────
struct amber_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;
    amber_value_widget_t *value;
    amber_value_widget_t *unit;
    amber_value_widget_t *ind_value[M_COUNT];
    lv_obj_t *icons[M_COUNT];
    float rpm;
    float rpm_smoothed;     // valeur affichée, lissée entre deux trames ECU
    uint32_t last_update_tick;
    bool has_snapshot;
    int32_t rpm_segment;
    int32_t rpm_display;
    int32_t coolant_display;
    int32_t battery_display;
    int32_t oil_display;
    bool connected_display;
    app_settings_units_t units;
    ecu_data_t latest_data;
    bool has_latest_data;
};

// Michroma n'existe qu'en une graisse : on simule le gras en superposant un
// second calque décalé (spread px de part et d'autre). L'écart dépend de la
// taille (trop d'écart sur un petit texte le ferait « doubler »).
#define BOLD_XL 2   // grande valeur (« 800 »)
#define BOLD_SM 1   // RPM / valeurs des indicateurs

static ui_layout_t layout_of(const lv_area_t *area) {
    ui_layout_t layout = ui_layout_fit(lv_area_get_width(area),
                                        lv_area_get_height(area));
    layout.ox += area->x1;
    layout.oy += area->y1;
    return layout;
}

static float circle_bottom_y(float x) {
    const float dx = x - DIAL_CX;
    return DIAL_CY + sqrtf(DIAL_CX * DIAL_CX - dx * dx);
}

static float separator_bottom_y(float x) {
    const float reference_gap = circle_bottom_y(VSEP_REF_X) - VSEP_REF_BOTTOM;
    return circle_bottom_y(x) - reference_gap;
}

// ── 3. DESSIN : primitives ───────────────────────────────────────────────────

static void draw_line(lv_layer_t *l, float x1, float y1, float x2, float y2,
                      float w, lv_color_t c) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = c;
    d.opa = LV_OPA_COVER;
    d.width = LV_MAX(1, (int32_t)lroundf(w));
    d.round_start = 0;
    d.round_end = 0;
    d.p1.x = x1; d.p1.y = y1;
    d.p2.x = x2; d.p2.y = y2;
    lv_draw_line(l, &d);
}

// Dessine un segment annulaire en une seule passe via lv_draw_arc
static void draw_arc_bar(lv_layer_t *l, int32_t cx, int32_t cy,
                         int32_t r_out, int32_t thickness,
                         float a0, float a1, lv_color_t c) {
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center.x = cx;
    d.center.y = cy;
    d.radius = r_out;
    d.width = thickness;
    d.start_angle = a0;
    d.end_angle = a1;
    d.color = c;
    d.opa = LV_OPA_COVER;
    d.rounded = 0; // Extrémités coupées radialement droites
    lv_draw_arc(l, &d);
}

static void canvas_draw_cb(lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_target(e);
    lv_layer_t *layer = lv_event_get_layer(e);

    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const ui_layout_t t = layout_of(&a);

    const int32_t cx = (int32_t)lroundf(ui_layout_x(&t, DIAL_CX));
    const int32_t cy = (int32_t)lroundf(ui_layout_y(&t, DIAL_CY));
    const int32_t r_in  = (int32_t)lroundf(DIAL_R_IN * t.scale);
    const int32_t r_out = (int32_t)lroundf(DIAL_R_OUT * t.scale);
    const int32_t bar_thickness = r_out - r_in;

    // Répartition uniforme stricte : 26 barres et 25 interstices égaux
    const float total_gaps = (float)(SEG_COUNT - 1) * SEG_GAP_DEG;
    const float bar_deg = (DIAL_SWEEP - total_gaps) / (float)SEG_COUNT;
    const float pitch_deg = bar_deg + SEG_GAP_DEG;

    amber_screen_t *scr = lv_obj_get_user_data(obj);
    if (scr == NULL) return;

    const float prog = LV_CLAMP(0.0f, (scr->rpm - RPM_MIN) / (RPM_MAX - RPM_MIN), 1.0f);
    const int bright = (int)lroundf(prog * SEG_COUNT);

    // Dessin direct des 26 barres
    for (int i = 0; i < SEG_COUNT; i++) {
        const float a0 = DIAL_START + (float)i * pitch_deg;
        const float a1 = a0 + bar_deg;
        const lv_color_t col = (i < bright) ? ui_theme_amber_bright() : ui_theme_amber_dim();
        draw_arc_bar(layer, cx, cy, r_out, bar_thickness, a0, a1, col);
    }

    // (Pas d'aiguille : les barres allumées suffisent à indiquer le régime.)

    // Séparateur horizontal
    const float hy_out = ui_layout_y(&t, VSEP_TOP_OUT);
    const float hy_mid = ui_layout_y(&t, HSEP_Y);
    draw_line(layer, ui_layout_x(&t, HSEP_X1), hy_out,
              ui_layout_x(&t, 83.333f - SEP_GAP), hy_out,
              SEP_W * t.scale, ui_theme_amber_separator());
    draw_line(layer, ui_layout_x(&t, 83.333f + SEP_GAP), hy_mid,
              ui_layout_x(&t, 236.667f - SEP_GAP), hy_mid,
              SEP_W * t.scale, ui_theme_amber_separator());
    draw_line(layer, ui_layout_x(&t, 236.667f + SEP_GAP), hy_out,
              ui_layout_x(&t, HSEP_X2), hy_out,
              SEP_W * t.scale, ui_theme_amber_separator());

    // Séparateurs verticaux
    for (int i = 0; i < 3; i++) {
        const float x = kVSep[i][0];
        draw_line(layer, ui_layout_x(&t, x), ui_layout_y(&t, kVSep[i][1]),
                  ui_layout_x(&t, x), ui_layout_y(&t, separator_bottom_y(x)),
                  SEP_W * t.scale, ui_theme_amber_separator());
    }
}

// ── 2. CRÉATION ──────────────────────────────────────────────────────────────

amber_screen_t *amber_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    amber_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));
    scr->rpm_smoothed = NAN;

    scr->root = amber_ui_root_create(parent);
    if (scr->root == NULL) goto fail;
    parent = scr->root;
    const int32_t side = lv_obj_get_width(scr->root);
    const ui_layout_t layout = ui_layout_fit(side, side);
    const float k = layout.scale;

    scr->canvas = amber_ui_canvas_create(parent, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    scr->value = amber_value_widget_create(
        parent, ui_font_or(ui_font_xxl, amber_ui_font_hero()),
        RPM_VAL_X, RPM_VAL_Y, BOLD_XL, "0");
    if (scr->value == NULL) goto fail;

    scr->unit = amber_value_widget_create(
        parent, ui_font_or(ui_font_rpm_unit, amber_ui_font_value()),
        DIAL_CX, RPM_UNIT_Y, BOLD_SM, "RPM");
    if (scr->unit == NULL) goto fail;

    for (int i = 0; i < M_COUNT; i++) {
        const int32_t isz = (int32_t)lroundf(IND_ICON_SZ * k);
        scr->icons[i] = dash_icon_create(parent, kInd[i].icon, isz,
                                         ui_theme_amber_bright());
        if (scr->icons[i] == NULL) goto fail;
        amber_ui_place_centered(scr->icons[i], parent, kInd[i].x,
                                kInd[i].icon_y, 0.0f, 0.0f);

        scr->ind_value[i] = amber_value_widget_create(
            parent, amber_ui_font_value(),
            kInd[i].x, kInd[i].label_y, BOLD_SM, "");
        if (scr->ind_value[i] == NULL) goto fail;
    }

    return scr;

fail:
    amber_screen_destroy(scr);
    return NULL;
}

// ── 5. SETTERS ───────────────────────────────────────────────────────────────

static float temperature_display(float celsius, app_settings_units_t units) {
    return units == APP_SETTINGS_UNITS_IMPERIAL
               ? celsius * 9.0f / 5.0f + 32.0f : celsius;
}

void amber_screen_set_units(amber_screen_t *scr, app_settings_units_t units) {
    if (scr == NULL || units >= APP_SETTINGS_UNITS_COUNT || scr->units == units) {
        return;
    }
    scr->units = units;
    scr->has_snapshot = false;
    if (scr->has_latest_data) amber_screen_update(scr, &scr->latest_data);
}

// NAN (mesure indisponible) s'affiche « -- » plutôt que « nan°C ».
static void format_temperature(char *buf, size_t size, float value,
                               app_settings_units_t units) {
    if (!isfinite(value)) {
        snprintf(buf, size, "--");
        return;
    }
    snprintf(buf, size, "%.0f°%c", value,
             units == APP_SETTINGS_UNITS_IMPERIAL ? 'F' : 'C');
}

void amber_screen_update(amber_screen_t *scr, const ecu_data_t *d) {
    if (scr == NULL || d == NULL || scr->canvas == NULL || scr->value == NULL) return;
    scr->latest_data = *d;
    scr->has_latest_data = true;

    // Le compte-tours suit la cible avec un léger retard plutôt que par
    // paliers de 200 ms ; le premier échantillon (NAN initial) saute direct.
    const uint32_t now = lv_tick_get();
    scr->rpm_smoothed = value_smoothing_step(
        scr->rpm_smoothed, d->rpm, now - scr->last_update_tick,
        RPM_SMOOTHING_TAU_MS, 1.0f);
    scr->last_update_tick = now;
    const float rpm = scr->rpm_smoothed;

    const float coolant = temperature_display(d->coolant_temp, scr->units);
    const float oil = temperature_display(d->oil_temp, scr->units);
    const int32_t rpm_display = isfinite(rpm) ? (int32_t)lroundf(rpm) : INT32_MIN;
    const int32_t coolant_display = isfinite(coolant)
        ? (int32_t)lroundf(coolant) : INT32_MIN;
    const int32_t battery_display = isfinite(d->battery_voltage)
        ? (int32_t)lroundf(d->battery_voltage * 10.0f) : INT32_MIN;
    const int32_t oil_display = isfinite(oil)
        ? (int32_t)lroundf(oil) : INT32_MIN;
    const float progress = LV_CLAMP(0.0f, (rpm - RPM_MIN) /
                                    (RPM_MAX - RPM_MIN), 1.0f);
    const int32_t rpm_segment = isfinite(rpm)
        ? (int32_t)lroundf(progress * SEG_COUNT) : 0;
    const bool text_changed = !scr->has_snapshot ||
        rpm_display != scr->rpm_display ||
        coolant_display != scr->coolant_display ||
        battery_display != scr->battery_display ||
        oil_display != scr->oil_display ||
        d->connected != scr->connected_display;
    const bool dial_changed = !scr->has_snapshot || rpm_segment != scr->rpm_segment;
    if (!text_changed && !dial_changed) return;

    char buf[24];
    if (rpm_display != scr->rpm_display || !scr->has_snapshot) {
        if (isfinite(rpm)) snprintf(buf, sizeof(buf), "%.0f", rpm);
        else snprintf(buf, sizeof(buf), "--");
        amber_value_widget_set(scr->value, buf);
    }
    if (coolant_display != scr->coolant_display || !scr->has_snapshot) {
        format_temperature(buf, sizeof(buf), coolant, scr->units);
        amber_value_widget_set(scr->ind_value[M_COOLANT], buf);
    }
    if (battery_display != scr->battery_display || !scr->has_snapshot) {
        if (isfinite(d->battery_voltage)) {
            snprintf(buf, sizeof(buf), "%.1fV", d->battery_voltage);
        } else {
            snprintf(buf, sizeof(buf), "--");
        }
        amber_value_widget_set(scr->ind_value[M_BATTERY], buf);
    }
    if (oil_display != scr->oil_display || !scr->has_snapshot) {
        format_temperature(buf, sizeof(buf), oil, scr->units);
        amber_value_widget_set(scr->ind_value[M_OIL], buf);
    }
    if (d->connected != scr->connected_display || !scr->has_snapshot) {
        amber_value_widget_set(scr->ind_value[M_OBD], d->connected ? "OBD" : "--");
    }

    if (dial_changed) {
        scr->rpm = rpm;
        scr->rpm_segment = rpm_segment;
        lv_obj_invalidate(scr->canvas);
    }
    scr->rpm_display = rpm_display;
    scr->coolant_display = coolant_display;
    scr->battery_display = battery_display;
    scr->oil_display = oil_display;
    scr->connected_display = d->connected;
    scr->has_snapshot = true;
}

void amber_screen_destroy(amber_screen_t *scr) {
    if (scr == NULL) return;

    amber_value_widget_destroy(scr->value);
    amber_value_widget_destroy(scr->unit);
    for (int i = 0; i < M_COUNT; i++) {
        amber_value_widget_destroy(scr->ind_value[i]);
        if (scr->icons[i] != NULL) lv_obj_delete(scr->icons[i]);
    }
    // The root owns the drawing object; wrappers above own their labels.
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
