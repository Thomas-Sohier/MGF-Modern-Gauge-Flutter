#pragma once

#include <stdbool.h>

#include "domain/ecu_data.h"
#include "domain/mems_protocol.h"

// Projection de l'instantané MEMS décodé (`mems_data_t`, couche protocole)
// vers le modèle consommé par l'écran (`ecu_data_t`).
//
// Correspondances : rpm/températures/battery_voltage directs (températures
// NAN si le capteur est signalé non supporté, brut 0xFF) ; throttle estimé
// depuis la tension du potentiomètre papillon (0.6 V fermé .. 4.6 V plein gaz,
// approx.) ; `connected` fixé par l'appelant selon l'état de la liaison.
//
// Les champs sans équivalent MEMS valent NAN — en particulier `oil_temp`
// (pas de sonde d'huile). Les bits DTC sont projetés dans `fault_flags`.
void mems_to_ecu_data(const mems_data_t *in, bool connected, ecu_data_t *out);
