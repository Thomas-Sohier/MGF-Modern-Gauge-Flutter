#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"

// Écran admission ambre pour le cadran rond 480 x 480. La composition
// respecte une safe area circulaire ; géométrie, textes et objets LVGL restent
// privés à l'implémentation.
typedef struct admission_screen_s admission_screen_t;

// `parent` doit rester vivant jusqu'à admission_screen_destroy(). Les appels
// sont à effectuer dans le thread LVGL (ou sous son verrou).
admission_screen_t *admission_screen_create(lv_obj_t *parent);
void admission_screen_update(admission_screen_t *scr, const ecu_data_t *data);
void admission_screen_destroy(admission_screen_t *scr);
