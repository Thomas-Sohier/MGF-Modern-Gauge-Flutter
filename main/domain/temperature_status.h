#pragma once

#include "domain/ecu_data.h"

// Seuils de température (Celsius) du cadran ambre. L'eau est la mesure héros ;
// l'huile ne participe qu'à l'alerte thermique haute.
#define TEMPERATURE_COOLANT_COLD_C   70.0f
#define TEMPERATURE_COOLANT_DANGER_C 105.0f
#define TEMPERATURE_OIL_DANGER_C     130.0f

// Ligne d'état de l'écran températures, par ordre de priorité strict :
// déconnexion > alerte thermique (eau ou huile) > défaut sonde eau >
// eau indisponible > moteur froid > températures OK.
typedef enum {
    TEMPERATURE_STATUS_NO_LINK,
    TEMPERATURE_STATUS_THERMAL_ALERT,
    TEMPERATURE_STATUS_COOLANT_SENSOR_FAULT,
    TEMPERATURE_STATUS_COOLANT_UNAVAILABLE,
    TEMPERATURE_STATUS_COLD,
    TEMPERATURE_STATUS_OK,
} temperature_status_t;

// Détermine l'état affiché à partir de l'instantané ECU. Fonction pure : ne
// touche à aucune mesure et ne déduit jamais la normalité de l'eau d'une autre
// sonde. Une sonde indisponible vaut NAN.
static inline temperature_status_t
temperature_status_of(const ecu_data_t *data) {
    if (data == NULL || !data->connected) {
        return TEMPERATURE_STATUS_NO_LINK;
    }

    const bool coolant_ok = isfinite(data->coolant_temp);
    const bool oil_ok = isfinite(data->oil_temp);
    const bool hot =
        (coolant_ok && data->coolant_temp >= TEMPERATURE_COOLANT_DANGER_C) ||
        (oil_ok && data->oil_temp >= TEMPERATURE_OIL_DANGER_C);
    if (hot) {
        return TEMPERATURE_STATUS_THERMAL_ALERT;
    }

    if (data->faults_available &&
        (data->fault_flags & ECU_FAULT_COOLANT_SENSOR)) {
        return TEMPERATURE_STATUS_COOLANT_SENSOR_FAULT;
    }

    if (!coolant_ok) {
        return TEMPERATURE_STATUS_COOLANT_UNAVAILABLE;
    }

    if (data->coolant_temp < TEMPERATURE_COOLANT_COLD_C) {
        return TEMPERATURE_STATUS_COLD;
    }

    return TEMPERATURE_STATUS_OK;
}
