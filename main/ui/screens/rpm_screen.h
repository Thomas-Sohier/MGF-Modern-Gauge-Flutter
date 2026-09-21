#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"

// Construit l'écran RPM principal (jauge double arc + valeur centrale +
// indicateurs de métriques en arc) dans `parent`. Reproduit rpm_screen.dart.
// Retourne un handle opaque à passer à rpm_screen_update().
typedef struct rpm_screen_s rpm_screen_t;

rpm_screen_t *rpm_screen_create(lv_obj_t *parent);

// Rafraîchit toutes les valeurs affichées depuis un instantané ECU.
void rpm_screen_update(rpm_screen_t *scr, const ecu_data_t *d);
