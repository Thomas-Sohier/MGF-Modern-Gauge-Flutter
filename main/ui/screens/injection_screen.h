#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"

// Écran d'injection ambre pour le panneau rond 480 x 480. Le handle reste
// opaque afin que la géométrie et les buffers de texte restent privés à l'écran.
typedef struct injection_screen_s injection_screen_t;

// `parent` doit rester vivant jusqu'à injection_screen_destroy(). Les appels
// sont à effectuer dans le thread LVGL (ou sous son verrou).
injection_screen_t *injection_screen_create(lv_obj_t *parent);
void injection_screen_update(injection_screen_t *scr, const ecu_data_t *data);
void injection_screen_destroy(injection_screen_t *scr);
