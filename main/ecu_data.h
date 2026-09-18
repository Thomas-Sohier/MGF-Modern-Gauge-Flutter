#pragma once

#include <stdbool.h>

// Instantané des données ECU affichées par l'écran RPM.
// Équivalent réduit de EcuInfos/DialData côté Flutter (lib/models/ecu_data.dart),
// limité aux champs consommés par l'écran principal.
typedef struct {
    bool  connected;      // liaison OBD active
    float rpm;            // régime moteur (tr/min)
    float throttle;       // angle papillon (0..100 %)
    float coolant_temp;   // température liquide de refroidissement (°C) — "LDR"
    float battery_voltage;// tension batterie (V)
    float oil_temp;       // température huile (°C)
} ecu_data_t;

// Source de données factice pour le test de faisabilité : un modèle de montée/
// descente de régime pilote rpm + throttle, les autres champs restent réalistes.
// Démarre un timer LVGL qui met à jour l'instantané et le pousse à l'écran RPM.
void fake_ecu_start(void);

// Instantané courant (thread LVGL uniquement).
const ecu_data_t *fake_ecu_current(void);
