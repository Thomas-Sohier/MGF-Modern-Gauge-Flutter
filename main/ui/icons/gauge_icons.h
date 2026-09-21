#pragma once

#include "lvgl.h"

// Petit widget d'icône vectorielle monochrome, dessiné via un événement LVGL
// (pas de police d'icônes) : reproduit les pictogrammes des captures MGF
// (thermomètre liquide, burette d'huile, batterie, prise OBD, compte-tours).
typedef enum {
    MGF_ICON_NONE = 0,
    MGF_ICON_COOLANT, // thermomètre  -> LDR / température liquide
    MGF_ICON_OIL,     // burette      -> température huile
    MGF_ICON_BATTERY, // batterie     -> tension
    MGF_ICON_OBD,     // maillons      -> liaison OBD
    MGF_ICON_TACH,    // compte-tours -> RPM
} mgf_icon_t;

// Crée une icône carrée de côté `size` (px) comme enfant de `parent`.
lv_obj_t *mgf_icon_create(lv_obj_t *parent, mgf_icon_t id, int32_t size);

// Change la couleur du tracé.
void mgf_icon_set_color(lv_obj_t *icon, lv_color_t color);
