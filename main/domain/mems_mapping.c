#include "domain/mems_mapping.h"

#include <stddef.h>

void mems_to_ecu_data(const mems_data_t *in, bool connected, ecu_data_t *out) {
    if (in == NULL || out == NULL) return;

    // Les champs non fournis par MEMS (injection, trims, ralenti...) restent
    // explicitement indisponibles plutôt que non initialisés.
    *out = ecu_data_unavailable();
    out->connected = connected;
    out->rpm = (float)in->engine_rpm;
    out->coolant_temp = in->coolant_temp;
    out->battery_voltage = in->battery_voltage;
    out->ambient_temp = in->ambient_temp;
    out->intake_air_temp = in->intake_air_temp;
    out->fuel_rail_temp = in->fuel_temp;
    out->map_sensor_kpa = in->map_kpa;
    out->throttle_pot_voltage = in->throttle_pot_voltage;
    out->ignition_advance = in->ignition_advance;
    out->coil_1_charge_time = in->coil_time;
    out->coil_2_charge_time = in->coil_time;
    out->coil_time_microseconds = in->coil_time * 1000.0f;
    out->estimated_air_fuel = in->air_fuel_ratio;
    out->lambda_mv = (float)in->lambda_voltage_mv;
    out->o2_mv = (float)in->lambda_voltage_mv;
    out->idle_valve_position = (float)in->iac_position;

    out->faults_available = true;
    out->fault_flags =
        (in->coolant_sensor_fault ? ECU_FAULT_COOLANT_SENSOR : 0u) |
        (in->intake_air_sensor_fault ? ECU_FAULT_INTAKE_AIR_SENSOR : 0u) |
        (in->fuel_pump_fault ? ECU_FAULT_FUEL_PUMP : 0u) |
        (in->throttle_pot_fault ? ECU_FAULT_THROTTLE_POT : 0u);

    // Estimation du % de gaz depuis la tension du potentiomètre papillon
    // (le pourcentage n'est pas fourni tel quel par MEMS). Plage nominale
    // ~0.6 V (fermé) .. ~4.6 V (plein gaz).
    const float lo = 0.6f, hi = 4.6f;
    float pct = (in->throttle_pot_voltage - lo) / (hi - lo) * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    out->throttle = pct;

    // MEMS 1.6/1.9 : pas de sonde de température d'huile. oil_temp reste NAN
    // (affiché « -- ») plutôt qu'un faux 0 °C permanent.
}
