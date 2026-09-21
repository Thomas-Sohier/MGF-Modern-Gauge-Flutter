#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef struct {
    lv_obj_t *front;
    lv_obj_t *shadow;
    lv_obj_t *parent;
    float x;
    float y;
    float width;
    float spread;
} amber_text_t;

// Texte ambre à deux calques. spread est exprimé en coordonnées logiques 320.
bool amber_text_create(amber_text_t *text, lv_obj_t *parent,
                       const lv_font_t *font, lv_color_t front_color,
                       lv_color_t shadow_color, const char *initial,
                       float x, float y, float width, float spread);
bool amber_text_valid(const amber_text_t *text);
void amber_text_place(amber_text_t *text);
void amber_text_set_static(amber_text_t *text, const char *value);
void amber_text_set_colors(amber_text_t *text, lv_color_t front,
                           lv_color_t shadow);
// Oublie les handles. Les objets restent possédés et détruits par leur parent.
void amber_text_clear(amber_text_t *text);
