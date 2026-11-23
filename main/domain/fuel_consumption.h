#pragma once

#include "domain/ecu_data.h"

#include <math.h>

// Estimation Speed-Density (pression-régime) de la consommation instantanée en
// litres par heure pour le moteur Rover K-Series 1.8 L (1796 cm³) piloté par
// MEMS 1.6 / 1.9.
//
// MEMS 1.x ne remonte ni la vitesse véhicule, ni l'odomètre, ni la largeur
// d'impulsion d'injecteur sur la K-line : le débit horaire est donc dérivé des
// grandeurs physiques mesurées par l'ECU (régime, pression collecteur MAP,
// température d'air d'admission IAT et rapport air/carburant AFR).
#define FUEL_ENGINE_DISPLACEMENT_M3 0.001796f
#define FUEL_VOLUMETRIC_EFFICIENCY  0.85f
#define FUEL_AIR_GAS_CONSTANT       287.05f
#define FUEL_GASOLINE_DENSITY_KG_L  0.745f
#define FUEL_DEFAULT_IAT_C          25.0f
#define FUEL_MIN_VALID_IAT_C        (-40.0f)
#define FUEL_MAX_VALID_IAT_C        120.0f
#define FUEL_DEFAULT_AFR            14.7f
#define FUEL_MIN_VALID_AFR          8.0f
#define FUEL_MAX_VALID_AFR          22.0f

// Calcule le débit instantané de carburant en L/h à partir d'un instantané ECU.
// Renvoie NAN si l'ECU est déconnectée ou si le régime ou la pression MAP sont
// indisponibles/invalides. Un moteur arrêté (0 tr/min) avec MAP valide consomme
// 0.0 L/h. Si l'IAT ou l'AFR sont absents (ex. capteur non supporté ou trame
// 0x7D pas encore reçue), des valeurs physiques nominales de repli (25 °C,
// 14.7 AFR) sont utilisées comme le fait le calculateur MEMS.
static inline float fuel_consumption_lph(const ecu_data_t *data) {
    if (data == NULL || !data->connected) {
        return NAN;
    }
    if (!isfinite(data->rpm) || data->rpm < 0.0f) {
        return NAN;
    }
    if (!isfinite(data->map_sensor_kpa) || data->map_sensor_kpa <= 0.0f) {
        return NAN;
    }
    if (data->rpm == 0.0f) {
        return 0.0f;
    }

    const float iat_c = (isfinite(data->intake_air_temp) &&
                         data->intake_air_temp >= FUEL_MIN_VALID_IAT_C &&
                         data->intake_air_temp <= FUEL_MAX_VALID_IAT_C)
                            ? data->intake_air_temp
                            : FUEL_DEFAULT_IAT_C;
    const float afr = (isfinite(data->estimated_air_fuel) &&
                       data->estimated_air_fuel >= FUEL_MIN_VALID_AFR &&
                       data->estimated_air_fuel <= FUEL_MAX_VALID_AFR)
                          ? data->estimated_air_fuel
                          : FUEL_DEFAULT_AFR;

    const float iat_k = iat_c + 273.15f;
    const float air_density_kg_m3 =
        (data->map_sensor_kpa * 1000.0f) / (FUEL_AIR_GAS_CONSTANT * iat_k);
    // 4 temps : 1 cycle d'admission tous les 2 tours, soit 30 * RPM cycles/h.
    const float air_mass_kg_h = 30.0f * data->rpm *
                                FUEL_ENGINE_DISPLACEMENT_M3 *
                                FUEL_VOLUMETRIC_EFFICIENCY * air_density_kg_m3;
    return air_mass_kg_h / (afr * FUEL_GASOLINE_DENSITY_KG_L);
}
