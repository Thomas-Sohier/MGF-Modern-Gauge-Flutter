#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"

// Écran ambre dédié aux mesures lambda / AFR. Le handle reste opaque afin que
// la géométrie et les buffers d'affichage restent privés à cette vue.
typedef struct lambda_screen_s lambda_screen_t;

// `parent` doit rester vivant jusqu'à lambda_screen_destroy(). Les appels sont
// à effectuer dans le thread LVGL (ou sous son verrou).
lambda_screen_t *lambda_screen_create(lv_obj_t *parent);
void lambda_screen_update(lambda_screen_t *screen, const ecu_data_t *data);
void lambda_screen_destroy(lambda_screen_t *screen);
