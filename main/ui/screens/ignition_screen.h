#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"

// Cadran ambre rond dédié à la télémétrie d'allumage. Le type reste opaque :
// toute la hiérarchie LVGL est détenue par l'écran jusqu'à sa destruction.
typedef struct ignition_screen_s ignition_screen_t;

// `parent` doit rester vivant jusqu'à ignition_screen_destroy(). Les appels
// doivent être effectués dans le thread LVGL (ou sous son verrou).
ignition_screen_t *ignition_screen_create(lv_obj_t *parent);
void ignition_screen_update(ignition_screen_t *screen, const ecu_data_t *data);
void ignition_screen_destroy(ignition_screen_t *screen);
