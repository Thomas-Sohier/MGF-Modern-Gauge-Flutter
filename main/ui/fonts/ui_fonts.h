#pragma once

#include "lvgl.h"
#include <stddef.h>

// Police Michroma rendue à la volée via tiny_ttf.
// Restent NULL tant que ui_fonts_init() n'a pas reçu les données TTF ; les
// écrans retombent alors sur Montserrat.
extern const lv_font_t *ui_font_xl; // grande valeur (RPM)
extern const lv_font_t *ui_font_l;  // valeurs des indicateurs
extern const lv_font_t *ui_font_m;  // libellés / unités

// Crée les trois tailles à partir d'un buffer TTF en mémoire (données conservées
// telles quelles par tiny_ttf : ne pas libérer le buffer). Idempotent.
void ui_fonts_init(const void *ttf_data, size_t ttf_size);

// Renvoie `f` si non NULL, sinon `fallback`.
static inline const lv_font_t *ui_font_or(const lv_font_t *f,
                                          const lv_font_t *fallback) {
    return f ? f : fallback;
}
