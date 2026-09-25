#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"
#include "ui/ui_layout.h"

// Kit de composition commun à tous les écrans ambre (hors compte-tours).
//
// Grammaire visuelle partagée, dans le repère logique 320 :
//   - couronne segmentée de 220° ouverte en bas (même rythme angulaire que les
//     barres du compte-tours) pour les écrans à jauge, sinon filet fin ;
//   - titre en capitales espacées, encadré de deux filets pointés ;
//   - valeur héros + unité réduite + légende ;
//   - grille 2×2 à séparateurs fins non jointifs (esprit cellules du RPM) ;
//   - ligne d'état dans l'ouverture basse de la couronne.
// Hiérarchie colorimétrique : valeur = ambre vif, légendes/unités = ambre
// séparateur (lisible), éléments éteints ou indisponibles = ambre sombre.

// ── Géométrie partagée ──────────────────────────────────────────────────────
#define AMBER_KIT_CX           160.0f
#define AMBER_KIT_RIM_START    160.0f
#define AMBER_KIT_RIM_SWEEP    220.0f
#define AMBER_KIT_RIM_R_OUT    157.0f
#define AMBER_KIT_RIM_R_IN     145.0f
#define AMBER_KIT_RIM_GAP_DEG  1.8f
#define AMBER_KIT_RIM_SEGMENTS 36

#define AMBER_KIT_TITLE_Y    47.0f
#define AMBER_KIT_HERO_Y     91.0f
#define AMBER_KIT_HERO_CAP_Y 123.0f
#define AMBER_KIT_GRID_TOP_Y 140.0f
#define AMBER_KIT_ROW1_Y     163.0f
#define AMBER_KIT_ROW1_CAP_Y 184.0f
#define AMBER_KIT_GRID_MID_Y 200.0f
#define AMBER_KIT_ROW2_Y     219.0f
#define AMBER_KIT_ROW2_CAP_Y 240.0f
#define AMBER_KIT_GRID_BOT_Y 252.0f
#define AMBER_KIT_STATUS_Y   272.0f
#define AMBER_KIT_COL_L      101.0f
#define AMBER_KIT_COL_R      219.0f

// ── Polices ─────────────────────────────────────────────────────────────────
const lv_font_t *amber_kit_font_hero(void);    // 56 px
const lv_font_t *amber_kit_font_value(void);   // 30 px
const lv_font_t *amber_kit_font_label(void);   // 22 px
const lv_font_t *amber_kit_font_caption(void); // 20 px

// ── Texte ───────────────────────────────────────────────────────────────────
typedef enum {
    AMBER_ALIGN_CENTER = 0,
    AMBER_ALIGN_LEFT,
    AMBER_ALIGN_RIGHT,
} amber_align_t;

// Label transparent. Les légendes (caption) reçoivent un léger interlettrage.
lv_obj_t *amber_kit_label(lv_obj_t *parent, const lv_font_t *font,
                          lv_color_t color, const char *text);
lv_obj_t *amber_kit_caption(lv_obj_t *parent, const char *text);
// Place le label de sorte que le milieu de ses capitales tombe en (x, y) :
// l'alignement optique ne dépend plus de l'interligne propre à chaque taille.
void amber_kit_place(lv_obj_t *label, float x, float y, amber_align_t align);
// Remplace le texte (copie) si différent ; retourne true si changé.
bool amber_kit_set_text(lv_obj_t *label, const char *text);

// Formate `value` avec `format`, ou "--" si indisponible.
void amber_kit_format(char *buffer, size_t size, bool available,
                      const char *format, float value);

// ── Lecture valeur + unité ──────────────────────────────────────────────────
// L'unité est plus petite et posée sur la ligne de base de la valeur. Une
// unité commençant par « ° » dessine un vrai anneau en exposant (Michroma
// rend U+00B0 comme un « o » bas de casse).
typedef struct {
    lv_obj_t *parent;
    lv_obj_t *value;
    lv_obj_t *unit;
    lv_obj_t *degree;
    float x;
    float y;
    bool available;
} amber_readout_t;

bool amber_readout_create(amber_readout_t *readout, lv_obj_t *parent,
                          const lv_font_t *value_font,
                          const lv_font_t *unit_font, float x, float y,
                          const char *unit);
// Retourne true si le rendu a changé.
bool amber_readout_set(amber_readout_t *readout, const char *text,
                       bool available);
void amber_readout_set_unit(amber_readout_t *readout, const char *unit);
void amber_readout_layout(amber_readout_t *readout);

// ── Primitives de dessin partagées ──────────────────────────────────────────
typedef enum {
    AMBER_RIM_NONE = 0,
    AMBER_RIM_SEGMENTS,
    AMBER_RIM_HAIRLINE,
} amber_rim_mode_t;

// Segments [lit_from, lit_to) allumés ; marker < 0 : pas de repère.
void amber_kit_draw_rim(lv_layer_t *layer, const ui_layout_t *layout,
                        int segments, int lit_from, int lit_to, int marker,
                        bool active);
void amber_kit_draw_hairline(lv_layer_t *layer, const ui_layout_t *layout);
// Filets pointés de part et d'autre d'un titre déjà placé.
void amber_kit_draw_title_rules(lv_layer_t *layer, const ui_layout_t *layout,
                                lv_obj_t *title, float y);
// Grille 2×2 ouverte : filet haut, croix centrale, marges aux intersections.
void amber_kit_draw_grid(lv_layer_t *layer, const ui_layout_t *layout);

// ── Gabarit de page instrument ──────────────────────────────────────────────
#define AMBER_PAGE_CELLS 4

typedef void (*amber_page_draw_cb_t)(lv_layer_t *layer,
                                     const ui_layout_t *layout, void *context);

typedef struct {
    const char *title;
    const char *hero_unit;
    const char *hero_caption;
    const char *cell_caption[AMBER_PAGE_CELLS];
    const char *cell_unit[AMBER_PAGE_CELLS];
    amber_rim_mode_t rim;
    int rim_segments;           // 0 -> AMBER_KIT_RIM_SEGMENTS
    bool no_grid;               // pas de cellules
    const lv_font_t *cell_font; // NULL -> police valeur (30 px)
    amber_page_draw_cb_t draw_extra;
    void *context;
} amber_page_spec_t;

typedef struct {
    lv_obj_t *root;
    lv_obj_t *canvas;
    lv_obj_t *title;
    amber_readout_t hero;
    lv_obj_t *hero_caption;
    amber_readout_t cell[AMBER_PAGE_CELLS];
    lv_obj_t *cell_caption[AMBER_PAGE_CELLS];
    lv_obj_t *status;
    amber_rim_mode_t rim;
    bool grid;
    int rim_segments;
    int rim_from;
    int rim_to;
    int rim_marker;
    bool rim_active;
    amber_page_draw_cb_t draw_extra;
    void *context;
} amber_page_t;

bool amber_page_create(amber_page_t *page, lv_obj_t *parent,
                       const amber_page_spec_t *spec);
// Fractions 0..1 de la couronne : segments allumés entre `from` et `to`
// (ordre libre), repère facultatif (`marker` < 0 : aucun). N'invalide le
// canvas que si l'état discret des segments change.
void amber_page_set_rim(amber_page_t *page, float from, float to, float marker,
                        bool active);
void amber_page_set_hero(amber_page_t *page, const char *text, bool available);
void amber_page_set_hero_caption(amber_page_t *page, const char *text,
                                 bool emphasized);
void amber_page_set_cell(amber_page_t *page, int index, const char *text,
                         bool available);
void amber_page_set_status(amber_page_t *page, const char *text,
                           bool emphasized);
void amber_page_invalidate(amber_page_t *page);
void amber_page_destroy(amber_page_t *page);
