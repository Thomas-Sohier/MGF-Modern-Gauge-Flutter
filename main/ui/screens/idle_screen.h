#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"

// Cadran ambre dédié à la régulation de ralenti. La structure est volontairement
// opaque : la géométrie, les buffers texte et l'état de rendu restent privés.
typedef struct idle_screen_s idle_screen_t;

// `parent` doit rester vivant jusqu'à idle_screen_destroy(). Les fonctions sont
// à appeler dans le thread LVGL (ou sous son verrou). La vue s'inscrit dans le
// plus grand carré disponible et utilise le repère logique ui_layout 320.
idle_screen_t *idle_screen_create(lv_obj_t *parent);
void idle_screen_update(idle_screen_t *screen, const ecu_data_t *data);
void idle_screen_destroy(idle_screen_t *screen);
