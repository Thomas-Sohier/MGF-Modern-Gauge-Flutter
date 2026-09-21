#pragma once

#include <stdbool.h>

// Instantané normalisé des données ECU consommées par les écrans. Les champs
// indisponibles sur une variante MEMS restent à zéro ; la structure est copiée
// en bloc entre la source, le contrôleur et les vues.
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
