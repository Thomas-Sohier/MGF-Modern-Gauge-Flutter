#pragma once

#include "lvgl.h"
#include "domain/app_settings.h"
#include "domain/ecu_data.h"

// Écran températures ambre pour le panneau rond 480 x 480. Le contenu est
// entièrement vectoriel : un anneau thermique principal et cinq cellules
// capteurs, dans le même repère logique que style_amber.c.
typedef struct temps_screen_s temps_screen_t;

temps_screen_t *temps_screen_create(lv_obj_t *parent);
void temps_screen_set_units(temps_screen_t *scr, app_settings_units_t units);
void temps_screen_update(temps_screen_t *scr, const ecu_data_t *data);
void temps_screen_destroy(temps_screen_t *scr);
