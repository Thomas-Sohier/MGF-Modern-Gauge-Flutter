#include "ui/widgets/amber_kit.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define DEGREE_UTF8 "\xC2\xB0"
#define CAPTION_LETTER_SPACE 1
#define UNIT_GAP 3.0f          // logique, entre valeur et unité
#define SEP_W 1.333f           // même trait que les séparateurs du RPM
#define SEP_GAP 5.0f           // marge aux intersections de la grille

// ── Polices ─────────────────────────────────────────────────────────────────
const lv_font_t *amber_kit_font_hero(void) {
    return ui_font_or(ui_font_xl, &lv_font_montserrat_48);
}

const lv_font_t *amber_kit_font_value(void) {
    return ui_font_or(ui_font_value, &lv_font_montserrat_20);
}

const lv_font_t *amber_kit_font_label(void) {
    return ui_font_or(ui_font_l, &lv_font_montserrat_20);
}

const lv_font_t *amber_kit_font_caption(void) {
    return ui_font_or(ui_font_m, &lv_font_montserrat_14);
}

// ── Métriques ───────────────────────────────────────────────────────────────
static ui_layout_t parent_layout(const lv_obj_t *object) {
    const lv_obj_t *parent = lv_obj_get_parent(object);
    return ui_layout_fit(lv_obj_get_width(parent), lv_obj_get_height(parent));
}

static int32_t font_ascent(const lv_font_t *font) {
    return font->line_height - font->base_line;
}

// Hauteur de capitale mesurée sur le glyphe « H » de la fonte réelle.
static int32_t font_cap(const lv_font_t *font) {
    lv_font_glyph_dsc_t glyph;
    if (lv_font_get_glyph_dsc(font, &glyph, 'H', 0) && glyph.box_h > 0) {
        return glyph.box_h + glyph.ofs_y;
    }
    return font->line_height * 7 / 10;
}

static const lv_font_t *label_font(const lv_obj_t *label) {
    return lv_obj_get_style_text_font(label, LV_PART_MAIN);
}

// ── Texte ───────────────────────────────────────────────────────────────────
lv_obj_t *amber_kit_label(lv_obj_t *parent, const lv_font_t *font,
                          lv_color_t color, const char *text) {
    if (parent == NULL || font == NULL) return NULL;
    lv_obj_t *label = lv_label_create(parent);
    if (label == NULL) return NULL;
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label, text != NULL ? text : "");
    return label;
}

lv_obj_t *amber_kit_caption(lv_obj_t *parent, const char *text) {
    lv_obj_t *label = amber_kit_label(parent, amber_kit_font_caption(),
                                      ui_theme_amber_separator(), text);
    if (label != NULL) {
        lv_obj_set_style_text_letter_space(label, CAPTION_LETTER_SPACE, 0);
    }
    return label;
}

void amber_kit_place(lv_obj_t *label, float x, float y, amber_align_t align) {
    if (label == NULL) return;
    const ui_layout_t layout = parent_layout(label);
    const lv_font_t *font = label_font(label);
    lv_obj_update_layout(label);
    const int32_t width = lv_obj_get_width(label);
    const float px = ui_layout_x(&layout, x);
    const float py = ui_layout_y(&layout, y);
    int32_t left = (int32_t)lroundf(px) - width / 2;
    if (align == AMBER_ALIGN_LEFT) left = (int32_t)lroundf(px);
    if (align == AMBER_ALIGN_RIGHT) left = (int32_t)lroundf(px) - width;
    const int32_t top = (int32_t)lroundf(py + font_cap(font) * 0.5f) -
                        font_ascent(font);
    lv_obj_set_pos(label, left, top);
}

static void set_text_color(lv_obj_t *label, lv_color_t color) {
    if (lv_color_eq(lv_obj_get_style_text_color(label, LV_PART_MAIN), color)) {
        return;
    }
    lv_obj_set_style_text_color(label, color, 0);
}

bool amber_kit_set_text(lv_obj_t *label, const char *text) {
    if (label == NULL) return false;
    const char *safe = text != NULL ? text : "";
    if (strcmp(lv_label_get_text(label), safe) == 0) return false;
    lv_label_set_text(label, safe);
    return true;
}

void amber_kit_format(char *buffer, size_t size, bool available,
                      const char *format, float value) {
    if (buffer == NULL || size == 0) return;
    if (available && isfinite(value)) snprintf(buffer, size, format, value);
    else snprintf(buffer, size, "--");
}

// ── Lecture valeur + unité ──────────────────────────────────────────────────
static const char *strip_degree(const char *unit, bool *degree) {
    *degree = unit != NULL && strncmp(unit, DEGREE_UTF8, 2) == 0;
    if (unit == NULL) return "";
    return *degree ? unit + 2 : unit;
}

static void readout_colors(amber_readout_t *readout) {
    const lv_color_t value = readout->available ? ui_theme_amber_bright()
                                                : ui_theme_amber_dim();
    const lv_color_t unit = readout->available ? ui_theme_amber_separator()
                                               : ui_theme_amber_dim();
    lv_obj_set_style_text_color(readout->value, value, 0);
    lv_obj_set_style_text_color(readout->unit, unit, 0);
    if (readout->degree != NULL) {
        // Anneau seul (ex. avance « 14° ») : il prolonge la valeur.
        const bool alone = lv_label_get_text(readout->unit)[0] == '\0';
        lv_obj_set_style_border_color(readout->degree, alone ? value : unit, 0);
    }
}

bool amber_readout_create(amber_readout_t *readout, lv_obj_t *parent,
                          const lv_font_t *value_font,
                          const lv_font_t *unit_font, float x, float y,
                          const char *unit) {
    if (readout == NULL || parent == NULL) return false;
    *readout = (amber_readout_t){.parent = parent, .x = x, .y = y};
    readout->value = amber_kit_label(parent, value_font, ui_theme_amber_dim(),
                                     "--");
    readout->unit = amber_kit_label(parent, unit_font, ui_theme_amber_dim(),
                                    "");
    readout->degree = lv_obj_create(parent);
    if (readout->value == NULL || readout->unit == NULL ||
        readout->degree == NULL) {
        return false;
    }
    lv_obj_remove_style_all(readout->degree);
    lv_obj_set_style_radius(readout->degree, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_opa(readout->degree, LV_OPA_COVER, 0);
    lv_obj_clear_flag(readout->degree,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    amber_readout_set_unit(readout, unit);
    return true;
}

void amber_readout_layout(amber_readout_t *readout) {
    if (readout == NULL || readout->value == NULL) return;
    const ui_layout_t layout = parent_layout(readout->value);
    const lv_font_t *vf = label_font(readout->value);
    const lv_font_t *uf = label_font(readout->unit);
    const bool has_degree = !lv_obj_has_flag(readout->degree,
                                             LV_OBJ_FLAG_HIDDEN);
    const bool has_unit = lv_label_get_text(readout->unit)[0] != '\0';

    lv_obj_update_layout(readout->value);
    lv_obj_update_layout(readout->unit);
    const int32_t value_w = lv_obj_get_width(readout->value);
    const int32_t unit_w = has_unit ? lv_obj_get_width(readout->unit) : 0;
    const int32_t value_cap = font_cap(vf);
    const int32_t unit_cap = font_cap(uf);
    const int32_t gap = (has_unit || has_degree)
        ? (int32_t)lroundf(UNIT_GAP * layout.scale) : 0;

    // L'anneau suit la capitale qu'il accompagne : celle de l'unité, ou celle
    // de la valeur lorsqu'il est seul (angle d'avance).
    const int32_t ring_ref = has_unit ? unit_cap : value_cap;
    const int32_t ring = has_degree ? LV_MAX(5, ring_ref * 45 / 100) : 0;
    const int32_t ring_w = LV_MAX(1, ring / 4);
    const int32_t ring_gap = has_degree && has_unit ? LV_MAX(1, ring / 3) : 0;

    const int32_t total = value_w + gap + ring + ring_gap + unit_w;
    const int32_t left = (int32_t)lroundf(ui_layout_x(&layout, readout->x)) -
                         total / 2;
    const int32_t baseline = (int32_t)lroundf(
        ui_layout_y(&layout, readout->y) + value_cap * 0.5f);

    lv_obj_set_pos(readout->value, left, baseline - font_ascent(vf));
    int32_t cursor = left + value_w + gap;
    if (has_degree) {
        lv_obj_set_size(readout->degree, ring, ring);
        lv_obj_set_style_border_width(readout->degree, ring_w, 0);
        lv_obj_set_pos(readout->degree, cursor, baseline - ring_ref);
        cursor += ring + ring_gap;
    }
    lv_obj_set_pos(readout->unit, cursor, baseline - font_ascent(uf));
}

void amber_readout_set_unit(amber_readout_t *readout, const char *unit) {
    if (readout == NULL || readout->unit == NULL) return;
    bool degree = false;
    const char *text = strip_degree(unit, &degree);
    lv_label_set_text(readout->unit, text);
    if (degree) lv_obj_remove_flag(readout->degree, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(readout->degree, LV_OBJ_FLAG_HIDDEN);
    readout_colors(readout);
    amber_readout_layout(readout);
}

bool amber_readout_set(amber_readout_t *readout, const char *text,
                       bool available) {
    if (readout == NULL || readout->value == NULL) return false;
    const bool text_changed = amber_kit_set_text(readout->value, text);
    const bool color_changed = readout->available != available;
    readout->available = available;
    if (color_changed) readout_colors(readout);
    if (text_changed) amber_readout_layout(readout);
    return text_changed || color_changed;
}

// ── Primitives de dessin ────────────────────────────────────────────────────
void amber_kit_draw_rim(lv_layer_t *layer, const ui_layout_t *layout,
                        int segments, int lit_from, int lit_to, int marker,
                        bool active) {
    if (segments <= 0) return;
    const float step = AMBER_KIT_RIM_SWEEP / (float)segments;
    const float width = AMBER_KIT_RIM_R_OUT - AMBER_KIT_RIM_R_IN;
    const float radius = AMBER_KIT_RIM_R_OUT;
    const lv_color_t on = ui_theme_amber_bright();
    const lv_color_t off = ui_theme_amber_dim();

    for (int i = 0; i < segments; i++) {
        const float a0 = AMBER_KIT_RIM_START + i * step +
                         AMBER_KIT_RIM_GAP_DEG * 0.5f;
        const float a1 = a0 + step - AMBER_KIT_RIM_GAP_DEG;
        const bool lit = active && i >= lit_from && i < lit_to;
        amber_draw_arc_wrapped(layer, layout, AMBER_KIT_CX, AMBER_KIT_CX,
                               radius, width, a0, a1 - a0, lit ? on : off,
                               false);
    }

    // Repère (consigne, stœchiométrie, seuil) : index intérieur triangulé
    // par un trait épais et une pointe fine, tous deux ambre vif.
    if (marker >= 0 && marker <= segments) {
        const float angle = AMBER_KIT_RIM_START + marker * step;
        const lv_color_t color = active ? on : ui_theme_amber_separator();
        amber_draw_tick(layer, layout, AMBER_KIT_CX, AMBER_KIT_CX, angle,
                        AMBER_KIT_RIM_R_IN - 9.0f, AMBER_KIT_RIM_R_IN - 2.5f,
                        3.2f, color, false);
    }

    // Butées de début/fin : petits traits radiaux séparateurs.
    const float ends[2] = {AMBER_KIT_RIM_START,
                           AMBER_KIT_RIM_START + AMBER_KIT_RIM_SWEEP};
    for (int i = 0; i < 2; i++) {
        amber_draw_tick(layer, layout, AMBER_KIT_CX, AMBER_KIT_CX, ends[i],
                        AMBER_KIT_RIM_R_IN - 6.0f, AMBER_KIT_RIM_R_IN - 2.0f,
                        SEP_W, ui_theme_amber_separator(), false);
    }
}

void amber_kit_draw_hairline(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t sep = ui_theme_amber_separator();
    const float r = (AMBER_KIT_RIM_R_OUT + AMBER_KIT_RIM_R_IN) * 0.5f;
    amber_draw_arc_wrapped(layer, layout, AMBER_KIT_CX, AMBER_KIT_CX, r, SEP_W,
                           AMBER_KIT_RIM_START, AMBER_KIT_RIM_SWEEP,
                           ui_theme_amber_dim(), false);
    // Graduations horaires discrètes : 9 index sur l'arc, extrémités marquées.
    for (int i = 0; i <= 8; i++) {
        const float angle = AMBER_KIT_RIM_START + i * AMBER_KIT_RIM_SWEEP / 8.0f;
        const bool end = i == 0 || i == 8;
        amber_draw_tick(layer, layout, AMBER_KIT_CX, AMBER_KIT_CX, angle,
                        end ? AMBER_KIT_RIM_R_IN - 2.0f : r - 3.0f,
                        end ? AMBER_KIT_RIM_R_OUT : r + 3.0f,
                        end ? 2.0f : SEP_W, sep, false);
    }
}

void amber_kit_draw_title_rules(lv_layer_t *layer, const ui_layout_t *layout,
                                lv_obj_t *title, float y) {
    if (title == NULL || lv_label_get_text(title)[0] == '\0') return;
    lv_area_t area;
    lv_obj_get_coords(title, &area);
    const float half = (float)lv_area_get_width(&area) * 0.5f / layout->scale;
    const lv_color_t sep = ui_theme_amber_separator();
    for (int side = -1; side <= 1; side += 2) {
        const float inner = AMBER_KIT_CX + side * (half + 6.0f);
        const float outer = AMBER_KIT_CX + side * (half + 17.0f);
        amber_draw_line(layer, layout, inner + side * 4.0f, y, outer, y, 1.0f,
                        sep, false);
        amber_draw_dot(layer, layout, inner, y, 1.3f, ui_theme_amber_bright());
    }
}

void amber_kit_draw_grid(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t sep = ui_theme_amber_separator();
    const float cx = AMBER_KIT_CX;
    // Filet supérieur : sépare la valeur héros des mesures secondaires.
    amber_draw_line(layer, layout, 52.0f, AMBER_KIT_GRID_TOP_Y, 268.0f,
                    AMBER_KIT_GRID_TOP_Y, SEP_W, sep, false);
    // Croix centrale ouverte.
    amber_draw_line(layer, layout, cx, AMBER_KIT_GRID_TOP_Y + SEP_GAP, cx,
                    AMBER_KIT_GRID_MID_Y - SEP_GAP, SEP_W, sep, false);
    amber_draw_line(layer, layout, cx, AMBER_KIT_GRID_MID_Y + SEP_GAP, cx,
                    AMBER_KIT_GRID_BOT_Y, SEP_W, sep, false);
    amber_draw_line(layer, layout, 62.0f, AMBER_KIT_GRID_MID_Y,
                    cx - SEP_GAP, AMBER_KIT_GRID_MID_Y, SEP_W, sep, false);
    amber_draw_line(layer, layout, cx + SEP_GAP, AMBER_KIT_GRID_MID_Y,
                    258.0f, AMBER_KIT_GRID_MID_Y, SEP_W, sep, false);
}

// ── Gabarit de page ─────────────────────────────────────────────────────────
static void page_draw_cb(lv_event_t *event) {
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    amber_page_t *page = lv_obj_get_user_data(canvas);
    if (layer == NULL || page == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);

    if (page->rim == AMBER_RIM_SEGMENTS) {
        amber_kit_draw_rim(layer, &layout, page->rim_segments, page->rim_from,
                           page->rim_to, page->rim_marker, page->rim_active);
    } else if (page->rim == AMBER_RIM_HAIRLINE) {
        amber_kit_draw_hairline(layer, &layout);
    }
    amber_kit_draw_title_rules(layer, &layout, page->title,
                               AMBER_KIT_TITLE_Y);
    if (page->grid) amber_kit_draw_grid(layer, &layout);
    if (page->draw_extra != NULL) {
        page->draw_extra(layer, &layout, page->context);
    }
}

bool amber_page_create(amber_page_t *page, lv_obj_t *parent,
                       const amber_page_spec_t *spec) {
    if (page == NULL || parent == NULL || spec == NULL) return false;
    *page = (amber_page_t){
        .rim = spec->rim,
        .grid = !spec->no_grid,
        .rim_segments = spec->rim_segments > 0 ? spec->rim_segments
                                               : AMBER_KIT_RIM_SEGMENTS,
        .rim_marker = -1,
        .draw_extra = spec->draw_extra,
        .context = spec->context,
    };

    page->root = amber_ui_root_create(parent);
    if (page->root == NULL) return false;
    page->canvas = amber_ui_canvas_create(page->root, page, page_draw_cb);
    if (page->canvas == NULL) return false;

    page->title = amber_kit_caption(page->root, spec->title);
    if (page->title == NULL) return false;
    lv_obj_set_style_text_letter_space(page->title, 2, 0);
    amber_kit_place(page->title, AMBER_KIT_CX, AMBER_KIT_TITLE_Y,
                    AMBER_ALIGN_CENTER);

    if (!amber_readout_create(&page->hero, page->root, amber_kit_font_hero(),
                              amber_kit_font_label(), AMBER_KIT_CX,
                              AMBER_KIT_HERO_Y, spec->hero_unit)) {
        return false;
    }
    page->hero_caption = amber_kit_caption(page->root, spec->hero_caption);
    if (page->hero_caption == NULL) return false;
    amber_kit_place(page->hero_caption, AMBER_KIT_CX, AMBER_KIT_HERO_CAP_Y,
                    AMBER_ALIGN_CENTER);

    if (page->grid) {
        static const float xs[AMBER_PAGE_CELLS] = {
            AMBER_KIT_COL_L, AMBER_KIT_COL_R, AMBER_KIT_COL_L, AMBER_KIT_COL_R};
        static const float ys[AMBER_PAGE_CELLS] = {
            AMBER_KIT_ROW1_Y, AMBER_KIT_ROW1_Y, AMBER_KIT_ROW2_Y,
            AMBER_KIT_ROW2_Y};
        static const float cap_ys[AMBER_PAGE_CELLS] = {
            AMBER_KIT_ROW1_CAP_Y, AMBER_KIT_ROW1_CAP_Y, AMBER_KIT_ROW2_CAP_Y,
            AMBER_KIT_ROW2_CAP_Y};
        for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
            if (!amber_readout_create(&page->cell[i], page->root,
                                      spec->cell_font != NULL
                                          ? spec->cell_font
                                          : amber_kit_font_value(),
                                      amber_kit_font_caption(), xs[i], ys[i],
                                      spec->cell_unit[i])) {
                return false;
            }
            page->cell_caption[i] = amber_kit_caption(page->root,
                                                      spec->cell_caption[i]);
            if (page->cell_caption[i] == NULL) return false;
            amber_kit_place(page->cell_caption[i], xs[i], cap_ys[i],
                            AMBER_ALIGN_CENTER);
        }
    }

    page->status = amber_kit_caption(page->root, "");
    if (page->status == NULL) return false;
    amber_kit_place(page->status, AMBER_KIT_CX, AMBER_KIT_STATUS_Y,
                    AMBER_ALIGN_CENTER);
    return true;
}

void amber_page_set_rim(amber_page_t *page, float from, float to, float marker,
                        bool active) {
    if (page == NULL || page->canvas == NULL) return;
    const int n = page->rim_segments;
    if (!isfinite(from)) from = 0.0f;
    if (!isfinite(to)) to = from;
    if (from > to) {
        const float swap = from;
        from = to;
        to = swap;
    }
    const int lit_from = (int)lroundf(amber_clampf(from, 0.0f, 1.0f) * n);
    const int lit_to = (int)lroundf(amber_clampf(to, 0.0f, 1.0f) * n);
    const int mark = marker >= 0.0f && isfinite(marker)
        ? (int)lroundf(amber_clampf(marker, 0.0f, 1.0f) * n) : -1;
    if (lit_from == page->rim_from && lit_to == page->rim_to &&
        mark == page->rim_marker && active == page->rim_active) {
        return;
    }
    page->rim_from = lit_from;
    page->rim_to = lit_to;
    page->rim_marker = mark;
    page->rim_active = active;
    lv_obj_invalidate(page->canvas);
}

void amber_page_set_hero(amber_page_t *page, const char *text, bool available) {
    if (page == NULL) return;
    amber_readout_set(&page->hero, text, available);
}

void amber_page_set_hero_caption(amber_page_t *page, const char *text,
                                 bool emphasized) {
    if (page == NULL || page->hero_caption == NULL) return;
    set_text_color(page->hero_caption, emphasized ? ui_theme_amber_bright()
                                           : ui_theme_amber_separator());
    if (amber_kit_set_text(page->hero_caption, text)) {
        amber_kit_place(page->hero_caption, AMBER_KIT_CX,
                        AMBER_KIT_HERO_CAP_Y, AMBER_ALIGN_CENTER);
    }
}

void amber_page_set_cell(amber_page_t *page, int index, const char *text,
                         bool available) {
    if (page == NULL || !page->grid || index < 0 || index >= AMBER_PAGE_CELLS) {
        return;
    }
    amber_readout_set(&page->cell[index], text, available);
}

void amber_page_set_status(amber_page_t *page, const char *text,
                           bool emphasized) {
    if (page == NULL || page->status == NULL) return;
    set_text_color(page->status, emphasized ? ui_theme_amber_bright()
                                           : ui_theme_amber_separator());
    if (amber_kit_set_text(page->status, text)) {
        amber_kit_place(page->status, AMBER_KIT_CX, AMBER_KIT_STATUS_Y,
                        AMBER_ALIGN_CENTER);
    }
}

void amber_page_invalidate(amber_page_t *page) {
    if (page != NULL && page->canvas != NULL) lv_obj_invalidate(page->canvas);
}

void amber_page_destroy(amber_page_t *page) {
    if (page == NULL) return;
    if (page->root != NULL) lv_obj_delete(page->root);
    *page = (amber_page_t){0};
}
