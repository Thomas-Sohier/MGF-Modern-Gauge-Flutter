#pragma once

#include "lvgl.h"
#include "domain/app_settings.h"
#include "domain/ecu_data.h"

// Variante visuelle « ambre » de l'écran RPM (cf. specs/image, photo 1) :
// fond brun-noir, monochrome ambre, arc segmenté fin, indicateurs icône +
// valeur. Même contenu/métriques que l'écran RPM de base.
typedef struct amber_screen_s amber_screen_t;

// `parent` doit rester vivant jusqu'à amber_screen_destroy(). Ces fonctions
// manipulent des objets LVGL et s'appellent dans le thread LVGL (ou sous son
// verrou). Racine privée carrée, centrée dans la zone de contenu du parent,
// sans padding. Géométrie mise à l'échelle à la création ; recréer l'écran
// après redimensionnement du parent. Les polices restent calibrées pour 480 px.
amber_screen_t *amber_screen_create(lv_obj_t *parent);
void amber_screen_set_units(amber_screen_t *scr, app_settings_units_t units);
void amber_screen_update(amber_screen_t *scr, const ecu_data_t *d);

// Détruit l'écran et libère son état. À appeler depuis le thread LVGL avant de
// détruire son parent.
void amber_screen_destroy(amber_screen_t *scr);
