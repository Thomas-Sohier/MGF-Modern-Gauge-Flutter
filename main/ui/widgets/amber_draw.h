#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "ui/ui_layout.h"

// Primitives vectorielles communes aux écrans ambre. Toutes les coordonnées
// sont exprimées dans le repère logique 320×320 puis transformées ici.
ui_layout_t amber_draw_layout(const lv_area_t *area);
float amber_clampf(float value, float low, float high);
float amber_progress(float value, float low, float high);

void amber_draw_line(lv_layer_t *layer, const ui_layout_t *layout,
                     float x1, float y1, float x2, float y2, float width,
                     lv_color_t color, bool rounded);
void amber_draw_arc(lv_layer_t *layer, const ui_layout_t *layout,
                    float cx, float cy, float radius, float width,
                    float start, float end, lv_color_t color, bool rounded);
void amber_draw_arc_wrapped(lv_layer_t *layer, const ui_layout_t *layout,
                            float cx, float cy, float radius, float width,
                            float start, float sweep, lv_color_t color,
                            bool rounded);
void amber_draw_circle(lv_layer_t *layer, const ui_layout_t *layout,
                       float cx, float cy, float radius, float width,
                       lv_color_t color);
void amber_draw_dot(lv_layer_t *layer, const ui_layout_t *layout,
                    float x, float y, float radius, lv_color_t color);
void amber_draw_tick(lv_layer_t *layer, const ui_layout_t *layout,
                     float cx, float cy, float angle_deg, float inner,
                     float outer, float width, lv_color_t color,
                     bool rounded);
