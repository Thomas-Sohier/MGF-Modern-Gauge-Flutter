#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"

// Écran de diagnostic monochrome ambre. Les codes détaillés ne font pas partie
// de l'instantané ECU embarqué : l'écran expose donc explicitement l'état de la
// liaison et évite de présenter une absence de codes comme un résultat de scan.
typedef struct faults_screen_s faults_screen_t;

// Crée une racine carrée centrée dans parent. Toutes les opérations LVGL sont
// à appeler dans le thread LVGL (ou sous son verrou).
faults_screen_t *faults_screen_create(lv_obj_t *parent);

// Actualise l'état de liaison sans créer ni détruire d'objet LVGL.
void faults_screen_update(faults_screen_t *scr, const ecu_data_t *data);

// Détruit l'écran et libère son état privé.
void faults_screen_destroy(faults_screen_t *scr);
