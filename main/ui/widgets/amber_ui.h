#pragma once

#include "lvgl.h"

// Fabrique commune des écrans ambre ronds. Elle centralise le clipping, le
// fond, le canvas transparent et la typographie afin que chaque écran ne
// conserve que sa composition métier.
lv_obj_t *amber_ui_root_create(lv_obj_t *parent);
lv_obj_t *amber_ui_canvas_create(lv_obj_t *root, void *context,
                                 lv_event_cb_t draw_callback);
lv_obj_t *amber_ui_label_create(lv_obj_t *parent, const lv_font_t *font,
                                lv_color_t color, const char *text,
                                float logical_width);

// Jetons typographiques communs. Ils garantissent les mêmes replis de police
// et la même hiérarchie sur tous les écrans ambre.
const lv_font_t *amber_ui_font_hero(void);
const lv_font_t *amber_ui_font_value(void);
const lv_font_t *amber_ui_font_caption(void);
float amber_ui_bold_spread(unsigned physical_pixels);

void amber_ui_place_centered(lv_obj_t *object, lv_obj_t *parent,
                             float x, float y, float logical_width,
                             float logical_x_offset);
