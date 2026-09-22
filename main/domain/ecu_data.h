#pragma once

#include <math.h>
#include <stdbool.h>

// Instantané normalisé des données ECU consommées par les écrans. Un champ
// indisponible (ECU déconnectée, mesure absente de la variante MEMS) vaut NAN :
// les écrans l'affichent « -- » au lieu d'une fausse valeur nulle. La structure
// est copiée en bloc entre la source, le contrôleur et les vues.
typedef struct {
    bool connected;
    float rpm;
    float throttle;
    float coolant_temp;
    float battery_voltage;
    float oil_temp;

    float ambient_temp;
    float intake_air_temp;
    float fuel_rail_temp;
    float map_sensor_kpa;
    float throttle_pot_voltage;

    float ignition_advance;
    float ignition_advance_offset;
    float coil_1_charge_time;
    float coil_2_charge_time;
    float coil_time_microseconds;

    float injector_1_pw;
    float injector_2_pw;
    float fuelling_feedback_percent;
    float short_term_trim_percent;
    float long_term_trim;

    float lambda_mv;
    float o2_mv;
    float estimated_air_fuel;
    float lambda_sensor_duty_cycle;

    float idle_setpoint;
    float idle_adjuster_rpm;
    float idle_error;
    float idle_valve_position;
    float idle_base_position;
} ecu_data_t;

// Instantané « rien de connu » : ECU déconnectée, toutes les mesures à NAN.
static inline ecu_data_t ecu_data_unavailable(void) {
    return (ecu_data_t){
        .connected = false,
        .rpm = NAN, .throttle = NAN, .coolant_temp = NAN,
        .battery_voltage = NAN, .oil_temp = NAN,
        .ambient_temp = NAN, .intake_air_temp = NAN, .fuel_rail_temp = NAN,
        .map_sensor_kpa = NAN, .throttle_pot_voltage = NAN,
        .ignition_advance = NAN, .ignition_advance_offset = NAN,
        .coil_1_charge_time = NAN, .coil_2_charge_time = NAN,
        .coil_time_microseconds = NAN,
        .injector_1_pw = NAN, .injector_2_pw = NAN,
        .fuelling_feedback_percent = NAN, .short_term_trim_percent = NAN,
        .long_term_trim = NAN,
        .lambda_mv = NAN, .o2_mv = NAN, .estimated_air_fuel = NAN,
        .lambda_sensor_duty_cycle = NAN,
        .idle_setpoint = NAN, .idle_adjuster_rpm = NAN, .idle_error = NAN,
        .idle_valve_position = NAN, .idle_base_position = NAN,
    };
}
