#include "style_amber.h"
#include "dash_icons.h"
#include "ui_fonts.h"

#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ── 1. CONSTANTES : couleurs ─────────────────────────────────────────────────
#define AMBER_BG      lv_color_hex(0x1B1712) // fond brun-noir
#define AMBER_BRIGHT  lv_color_hex(0xFFB51B) // ambre principal (actif, texte)
#define AMBER_DIM     lv_color_hex(0x756345) // graduations inactives
#define AMBER_SEP     lv_color_hex(0xB47A12) // séparateurs / traits fins

// ── 1. CONSTANTES : géométrie (repère 320 px) ────────────────────────────────
#define REF_SIZE      320.0f   // côté du repère de référence

// Compte-tours
#define DIAL_CX       160.0f   // centre de l'arc
#define DIAL_CY       155.0f
#define DIAL_R_IN     124.0f   // dimensionnement plein écran 480x480
#define DIAL_R_OUT    155.0f

#define SEG_COUNT     26       // 26 barres
#define SEG_GAP_DEG   1.80f    // espacement angulaire constant entre chaque barre

#define DIAL_START    180.0f   // 9h (horizontale gauche)
#define DIAL_SWEEP    180.0f   // demi-cercle jusqu'à 3h (horizontale droite)

// Plage régime
#define RPM_MIN       0.0f
#define RPM_MAX       8000.0f

// Traits de séparation
#define SEP_W         0.8f
#define SEP_GAP       3.5f

// Ligne horizontale
#define HSEP_Y        190.0f
#define HSEP_X1       30.0f
#define HSEP_X2       290.0f

// Séparateurs verticaux
#define VSEP_TOP_OUT  160.0f
#define VSEP_TOP_MID  (HSEP_Y + SEP_GAP)
static const float kVSep[3][3] = {
    {95.0f, VSEP_TOP_OUT, 280.0f},
    {160.0f, VSEP_TOP_MID, 288.0f},
    {225.0f, VSEP_TOP_OUT, 280.0f},
};

// Valeur centrale
#define RPM_VAL_X     162.0f
#define RPM_VAL_Y     142.0f
#define RPM_UNIT_Y    180.0f

// ── 1. CONSTANTES : indicateurs bas ──────────────────────────────────────────
enum { M_COOLANT = 0, M_BATTERY, M_OIL, M_OBD, M_COUNT };
typedef struct {
    dash_icon_type_t icon;
    float x, icon_y, label_y;
} ind_def_t;

static const ind_def_t kInd[M_COUNT] = {
    {DASH_ICON_COOLANT,  62.5f,  180.0f, 215.0f},
    {DASH_ICON_BATTERY,  127.5f, 210.0f, 245.0f},
    {DASH_ICON_OIL,      192.5f, 210.0f, 245.0f},
    {DASH_ICON_OBD_LINK, 257.5f,  180.0f, 215.0f},
};
#define IND_ICON_SZ   40.0f

// ── État / mise à l'échelle ──────────────────────────────────────────────────
struct amber_screen_s {
    lv_obj_t *canvas;
    lv_obj_t *value;
    lv_obj_t *value_sh;               // calque faux-gras
    lv_obj_t *ind_value[M_COUNT];
    lv_obj_t *ind_value_sh[M_COUNT];  // calques faux-gras
};

// Michroma n'existe qu'en une graisse : on simule le gras en superposant un
// second calque décalé (spread px de part et d'autre). L'écart dépend de la
// taille (trop d'écart sur un petit texte le ferait « doubler »).
#define BOLD_XL 2   // grande valeur (« 800 »)
#define BOLD_SM 1   // RPM / valeurs des indicateurs

static float rpm_ref = 0.0f;

typedef struct { float ox, oy, k; } xform_t;

static xform_t xform_of(const lv_area_t *a) {
    const float w = (float)lv_area_get_width(a), h = (float)lv_area_get_height(a);
    const float s = LV_MIN(w, h);
    xform_t t;
    t.k = s / REF_SIZE;
    t.ox = a->x1 + (w - s) * 0.5f;
    t.oy = a->y1 + (h - s) * 0.5f;
    return t;
}
static inline float PX(const xform_t *t, float x) { return t->ox + x * t->k; }
static inline float PY(const xform_t *t, float y) { return t->oy + y * t->k; }

// ── 3. DESSIN : primitives ───────────────────────────────────────────────────

static void draw_line(lv_layer_t *l, float x1, float y1, float x2, float y2,
                      float w, lv_color_t c) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = c;
    d.opa = LV_OPA_COVER;
    d.width = (int32_t)lroundf(w);
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
    const xform_t t = xform_of(&a);

    const int32_t cx = (int32_t)lroundf(PX(&t, DIAL_CX));
    const int32_t cy = (int32_t)lroundf(PY(&t, DIAL_CY));
    const int32_t r_in  = (int32_t)lroundf(DIAL_R_IN * t.k);
    const int32_t r_out = (int32_t)lroundf(DIAL_R_OUT * t.k);
    const int32_t bar_thickness = r_out - r_in;

    // Répartition uniforme stricte : 26 barres et 25 interstices égaux
    const float total_gaps = (float)(SEG_COUNT - 1) * SEG_GAP_DEG;
    const float bar_deg = (DIAL_SWEEP - total_gaps) / (float)SEG_COUNT;
    const float pitch_deg = bar_deg + SEG_GAP_DEG;

    const float prog = LV_CLAMP(0.0f, (rpm_ref - RPM_MIN) / (RPM_MAX - RPM_MIN), 1.0f);
    const int bright = (int)lroundf(prog * SEG_COUNT);

    // Dessin direct des 26 barres
    for (int i = 0; i < SEG_COUNT; i++) {
        const float a0 = DIAL_START + (float)i * pitch_deg;
        const float a1 = a0 + bar_deg;
        const lv_color_t col = (i < bright) ? AMBER_BRIGHT : AMBER_DIM;
        draw_arc_bar(layer, cx, cy, r_out, bar_thickness, a0, a1, col);
    }

    // (Pas d'aiguille : les barres allumées suffisent à indiquer le régime.)

    // Séparateur horizontal
    const float hy_out = PY(&t, VSEP_TOP_OUT);
    const float hy_mid = PY(&t, HSEP_Y);
    draw_line(layer, PX(&t, HSEP_X1), hy_out, PX(&t, 95.0f - SEP_GAP), hy_out, SEP_W * t.k, AMBER_SEP);
    draw_line(layer, PX(&t, 95.0f + SEP_GAP), hy_mid, PX(&t, 225.0f - SEP_GAP), hy_mid, SEP_W * t.k, AMBER_SEP);
    draw_line(layer, PX(&t, 225.0f + SEP_GAP), hy_out, PX(&t, HSEP_X2), hy_out, SEP_W * t.k, AMBER_SEP);

    // Séparateurs verticaux
    for (int i = 0; i < 3; i++) {
        draw_line(layer, PX(&t, kVSep[i][0]), PY(&t, kVSep[i][1]),
                  PX(&t, kVSep[i][0]), PY(&t, kVSep[i][2]), SEP_W * t.k, AMBER_SEP);
    }
}

// ── 2. CRÉATION ──────────────────────────────────────────────────────────────

static void place(lv_obj_t *o, lv_obj_t *parent, float rx, float ry) {
    const float w = (float)lv_obj_get_width(parent), h = (float)lv_obj_get_height(parent);
    const float s = LV_MIN(w, h), k = s / REF_SIZE;
    const float ox = (w - s) * 0.5f, oy = (h - s) * 0.5f;
    lv_obj_align(o, LV_ALIGN_TOP_LEFT, (int32_t)lroundf(ox + rx * k),
                 (int32_t)lroundf(oy + ry * k));
}

// Centre un label sur (rx,ry) en 320-ref, avec un décalage horizontal dx (px).
static void place_centered(lv_obj_t *lbl, lv_obj_t *parent, float rx, float ry,
                           int dx) {
    lv_obj_update_layout(lbl);
    const float w = (float)lv_obj_get_width(parent), h = (float)lv_obj_get_height(parent);
    const float s = LV_MIN(w, h), k = s / REF_SIZE;
    const float ox = (w - s) * 0.5f, oy = (h - s) * 0.5f;
    const int32_t lw = lv_obj_get_width(lbl), lh = lv_obj_get_height(lbl);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT,
                 (int32_t)lroundf(ox + rx * k) - lw / 2 + dx,
                 (int32_t)lroundf(oy + ry * k) - lh / 2);
}

static lv_obj_t *new_amber_label(lv_obj_t *parent, const lv_font_t *font,
                                 const char *txt) {
    lv_obj_t *lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, AMBER_BRIGHT, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl, txt);
    return lbl;
}

// Crée une paire faux-gras (calque décalé + label avant). Renvoie l'avant ;
// *shadow reçoit le calque (à mettre à jour en même temps, ou NULL si statique).
static lv_obj_t *make_bold(lv_obj_t *parent, const lv_font_t *font, float rx,
                           float ry, const char *txt, int spread,
                           lv_obj_t **shadow) {
    lv_obj_t *sh = new_amber_label(parent, font, txt); // dessous
    lv_obj_t *fr = new_amber_label(parent, font, txt); // dessus
    place_centered(sh, parent, rx, ry, +spread);
    place_centered(fr, parent, rx, ry, -spread);
    if (shadow) *shadow = sh;
    return fr;
}

amber_screen_t *amber_screen_create(lv_obj_t *parent) {
    amber_screen_t *scr = lv_malloc(sizeof(amber_screen_t));
    lv_memzero(scr, sizeof(*scr));

    lv_obj_set_style_bg_color(parent, AMBER_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);

    const float w = (float)lv_obj_get_width(parent), h = (float)lv_obj_get_height(parent);
    const float k = LV_MIN(w, h) / REF_SIZE;

    scr->canvas = lv_obj_create(parent);
    lv_obj_remove_style_all(scr->canvas);
    lv_obj_set_size(scr->canvas, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(scr->canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(scr->canvas, canvas_draw_cb, LV_EVENT_DRAW_MAIN, NULL);

    scr->value = make_bold(parent, ui_font_or(ui_font_xl, &lv_font_montserrat_48),
                           RPM_VAL_X, RPM_VAL_Y, "0", BOLD_XL, &scr->value_sh);
    make_bold(parent, ui_font_or(ui_font_m, &lv_font_montserrat_20),
              DIAL_CX, RPM_UNIT_Y, "RPM", BOLD_SM, NULL);

    for (int i = 0; i < M_COUNT; i++) {
        const int32_t isz = (int32_t)lroundf(IND_ICON_SZ * k);
        lv_obj_t *icon = dash_icon_create(parent, kInd[i].icon, isz, AMBER_BRIGHT);
        place(icon, parent, kInd[i].x - IND_ICON_SZ * 0.5f,
              kInd[i].icon_y - IND_ICON_SZ * 0.5f);

        scr->ind_value[i] = make_bold(
            parent, ui_font_or(ui_font_m, &lv_font_montserrat_20),
            kInd[i].x, kInd[i].label_y, "", BOLD_SM, &scr->ind_value_sh[i]);
    }

    return scr;
}

// ── 5. SETTERS ───────────────────────────────────────────────────────────────

// Met à jour le texte d'une paire faux-gras (avant + calque) et la recentre.
static void set_bold(lv_obj_t *front, lv_obj_t *shadow, lv_obj_t *parent,
                     float rx, float ry, int spread, const char *txt) {
    lv_label_set_text(front, txt);
    place_centered(front, parent, rx, ry, -spread);
    lv_label_set_text(shadow, txt);
    place_centered(shadow, parent, rx, ry, +spread);
}

void amber_screen_update(amber_screen_t *scr, const ecu_data_t *d) {
    lv_obj_t *parent = lv_obj_get_parent(scr->canvas);
    rpm_ref = d->rpm;
    lv_obj_invalidate(scr->canvas);

    char buf[24];
    snprintf(buf, sizeof(buf), "%.0f", d->rpm);
    set_bold(scr->value, scr->value_sh, parent, RPM_VAL_X, RPM_VAL_Y, BOLD_XL, buf);

    snprintf(buf, sizeof(buf), "%.0f°C", d->coolant_temp);
    set_bold(scr->ind_value[M_COOLANT], scr->ind_value_sh[M_COOLANT], parent,
             kInd[M_COOLANT].x, kInd[M_COOLANT].label_y, BOLD_SM, buf);
    snprintf(buf, sizeof(buf), "%.1fV", d->battery_voltage);
    set_bold(scr->ind_value[M_BATTERY], scr->ind_value_sh[M_BATTERY], parent,
             kInd[M_BATTERY].x, kInd[M_BATTERY].label_y, BOLD_SM, buf);
    snprintf(buf, sizeof(buf), "%.0f°C", d->oil_temp);
    set_bold(scr->ind_value[M_OIL], scr->ind_value_sh[M_OIL], parent,
             kInd[M_OIL].x, kInd[M_OIL].label_y, BOLD_SM, buf);
    set_bold(scr->ind_value[M_OBD], scr->ind_value_sh[M_OBD], parent,
             kInd[M_OBD].x, kInd[M_OBD].label_y, BOLD_SM, d->connected ? "OBD" : "--");
}
