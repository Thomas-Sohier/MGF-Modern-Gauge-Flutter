#pragma once

#include "lvgl.h"
#include "ecu_data.h"

// Variante visuelle « ambre » de l'écran RPM (cf. specs/image, photo 1) :
// fond noir, monochrome ambre, arc segmenté fin + aiguille, indicateurs
// icône + valeur. Même contenu/métriques que l'écran RPM de base.
typedef struct amber_screen_s amber_screen_t;

amber_screen_t *amber_screen_create(lv_obj_t *parent);
void amber_screen_update(amber_screen_t *scr, const ecu_data_t *d);
