#pragma once

#include <stdbool.h>

#include "domain/ecu_source.h"

typedef struct fake_ecu_s fake_ecu_t;

// Crée une source indépendante. Le modèle est mis à jour par un timer LVGL ;
// ces fonctions doivent être appelées dans le thread LVGL (ou sous son verrou).
fake_ecu_t *fake_ecu_create(void);

// Démarre/arrête le timer de cette instance.
bool fake_ecu_start(fake_ecu_t *ecu);
void fake_ecu_stop(fake_ecu_t *ecu);

// Détruit l'instance et son timer éventuel.
void fake_ecu_destroy(fake_ecu_t *ecu);

// Lit une copie de l'instantané courant, sans exposer l'état interne.
bool fake_ecu_read(const fake_ecu_t *ecu, ecu_data_t *out);

// Adaptateur vers l'abstraction commune des sources ECU.
ecu_source_t fake_ecu_source(fake_ecu_t *ecu);
