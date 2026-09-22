#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"

// Écran de diagnostic monochrome ambre. Affiche les défauts remontés par l'ECU
// (`ecu_data_t.fault_flags`) ; une ECU déconnectée ou une source sans état de
// défaut reste « DEFAUTS INCONNUS », jamais présentée comme « AUCUN DEFAUT ».
typedef struct faults_screen_s faults_screen_t;

// Crée une racine carrée centrée dans parent. Toutes les opérations LVGL sont
// à appeler dans le thread LVGL (ou sous son verrou).
faults_screen_t *faults_screen_create(lv_obj_t *parent);

// Actualise liaison et défauts sans créer ni détruire d'objet LVGL.
void faults_screen_update(faults_screen_t *scr, const ecu_data_t *data);

// Détruit l'écran et libère son état privé.
void faults_screen_destroy(faults_screen_t *scr);
