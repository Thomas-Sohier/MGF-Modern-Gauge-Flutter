#pragma once

#include <stdbool.h>

#include "domain/app_settings.h"

// Persistance NVS des réglages hors de la boucle LVGL.
//
// settings_store_save() prépare puis commit une transaction NVS : c'est une
// opération lente qui ne doit jamais bloquer la tâche UI. Ce module possède une
// tâche FreeRTOS unique chargée d'écrire les réglages hors LVGL, tandis que le
// callback retourné par ce module (utilisable directement comme
// settings_coordinator_save_cb_t, donc par settings_runtime_create) reste
// rapide et non bloquant.
//
// Contrat de settings_persistence_save() :
//   - retourne true UNIQUEMENT si `settings` est identique au dernier snapshot
//     RÉELLEMENT écrit par le worker et qu'aucune demande plus récente n'est en
//     file ou en cours ; le coordinateur peut alors effacer son état « dirty »
//     sans risquer qu'une écriture concurrente différente écrase ensuite la
//     valeur acquittée ;
//   - sinon, copie la demande dans une file FreeRTOS bornée à 1 (dernier-gagne)
//     et retourne false ; le coordinateur garde son état « dirty » et réessaie
//     après son anti-rebond (3 s) ;
//   - ne fait aucune I/O et ne bloque jamais : le mutex interne est pris avec
//     un timeout de 0 tick (un mutex occupé => false).
//
// Cycle de vie :
//   - settings_persistence_create() démarre la tâche (pile 4096, priorité 3) et
//     retourne NULL si une ressource FreeRTOS manque, après libération complète
//     (aucune fuite) ;
//   - settings_persistence_destroy() signale l'arrêt par un message dédié,
//     attend la fin réelle de la tâche (sémaphore) puis libère tout. À appeler
//     UNIQUEMENT après l'arrêt de tous les producteurs (coordinateur /
//     settings_runtime) : plus aucune sauvegarde ne doit pouvoir être soumise.
typedef struct settings_persistence_s settings_persistence_t;

settings_persistence_t *settings_persistence_create(void);

// Callback de sauvegarde (contrat ci-dessus). `context` est le pointeur
// retourné par settings_persistence_create().
bool settings_persistence_save(void *context, const app_settings_t *settings);

// Arrête le worker, attend sa fin puis libère le module. Sans effet sur NULL.
void settings_persistence_destroy(settings_persistence_t *persistence);
