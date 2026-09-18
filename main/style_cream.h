#pragma once

#include "lvgl.h"
#include "ecu_data.h"

// Variante visuelle « crème » de l'écran RPM (cf. specs/image, photo 2) :
// fond crème analogique, texte sombre, arc en graduations avec zone rouge en
// fin d'échelle, filet séparateur, indicateurs icône + valeur.
typedef struct cream_screen_s cream_screen_t;

cream_screen_t *cream_screen_create(lv_obj_t *parent);
void cream_screen_update(cream_screen_t *scr, const ecu_data_t *d);
