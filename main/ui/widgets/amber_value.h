#pragma once

#include "lvgl.h"

typedef struct amber_value_widget_s amber_value_widget_t;

// Valeur ambre centrée sur le repère 320 px (un seul label net : le faux-gras
// par calque décalé doublait le texte Michroma). `parent` doit rester vivant
// jusqu'à amber_value_widget_destroy(). À utiliser dans le thread LVGL.
amber_value_widget_t *amber_value_widget_create(lv_obj_t *parent,
                                                const lv_font_t *font,
                                                float ref_x, float ref_y,
                                                const char *text);

void amber_value_widget_set(amber_value_widget_t *value, const char *text);

// Détruit le label et libère le handle. À appeler dans le thread LVGL.
void amber_value_widget_destroy(amber_value_widget_t *value);
