#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"

// Écran de guidage ambre, vectoriel et adapté au panneau rond 480 x 480.
// Le scénario affiché reste volontairement local tant que ecu_data_t ne porte
// pas encore l'état de navigation.
typedef struct navigation_screen_s navigation_screen_t;

// `parent` doit rester vivant jusqu'à navigation_screen_destroy(). Les appels
// doivent être effectués dans le thread LVGL (ou sous son verrou).
navigation_screen_t *navigation_screen_create(lv_obj_t *parent);
void navigation_screen_update(navigation_screen_t *scr, const ecu_data_t *data);
void navigation_screen_destroy(navigation_screen_t *scr);
