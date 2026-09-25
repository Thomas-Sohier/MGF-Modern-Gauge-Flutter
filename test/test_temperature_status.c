#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "domain/temperature_status.h"

static ecu_data_t connected_base(void) {
    ecu_data_t d = ecu_data_unavailable();
    d.connected = true;
    return d;
}

static void test_disconnected_ignores_residual(void) {
    // Même avec des chiffres résiduels et un défaut, pas de liaison prime.
    ecu_data_t d = ecu_data_unavailable();
    d.connected = false;
    d.coolant_temp = 120.0f;
    d.oil_temp = 140.0f;
    d.faults_available = true;
    d.fault_flags = ECU_FAULT_COOLANT_SENSOR;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_NO_LINK);
    assert(temperature_status_of(NULL) == TEMPERATURE_STATUS_NO_LINK);
}

static void test_connected_all_nan(void) {
    const ecu_data_t d = connected_base();
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_COOLANT_UNAVAILABLE);
}

static void test_coolant_nan_other_probes_known(void) {
    // La normalité de l'eau ne se déduit jamais d'une autre sonde.
    ecu_data_t d = connected_base();
    d.oil_temp = 90.0f;
    d.intake_air_temp = 30.0f;
    d.ambient_temp = 22.0f;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_COOLANT_UNAVAILABLE);
}

static void test_coolant_sensor_fault(void) {
    ecu_data_t d = connected_base();
    d.faults_available = true;
    d.fault_flags = ECU_FAULT_COOLANT_SENSOR;
    // Eau indisponible : le défaut sonde prend le pas sur « indisponible ».
    assert(temperature_status_of(&d) ==
           TEMPERATURE_STATUS_COOLANT_SENSOR_FAULT);

    // Le défaut n'est retenu que si faults_available.
    d.faults_available = false;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_COOLANT_UNAVAILABLE);

    // Un défaut d'une autre sonde ne déclenche pas le défaut eau.
    d.faults_available = true;
    d.fault_flags = ECU_FAULT_INTAKE_AIR_SENSOR;
    d.coolant_temp = 90.0f;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_OK);
}

static void test_cold_threshold(void) {
    ecu_data_t d = connected_base();
    d.coolant_temp = 69.9f;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_COLD);
    d.coolant_temp = 70.0f;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_OK);
}

static void test_coolant_danger_threshold(void) {
    ecu_data_t d = connected_base();
    d.coolant_temp = 104.9f;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_OK);
    d.coolant_temp = 105.0f;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_THERMAL_ALERT);
}

static void test_oil_danger_threshold(void) {
    ecu_data_t d = connected_base();
    d.coolant_temp = 90.0f;
    d.oil_temp = 129.9f;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_OK);
    d.oil_temp = 130.0f;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_THERMAL_ALERT);
}

static void test_hot_beats_sensor_fault(void) {
    ecu_data_t d = connected_base();
    d.coolant_temp = 110.0f;
    d.faults_available = true;
    d.fault_flags = ECU_FAULT_COOLANT_SENSOR;
    assert(temperature_status_of(&d) == TEMPERATURE_STATUS_THERMAL_ALERT);

    // Huile seule en surchauffe, eau indisponible avec défaut sonde : alerte.
    const ecu_data_t oil_hot =
        (ecu_data_t){.connected = true,
                     .faults_available = true,
                     .fault_flags = ECU_FAULT_COOLANT_SENSOR,
                     .coolant_temp = NAN,
                     .oil_temp = 135.0f};
    assert(temperature_status_of(&oil_hot) == TEMPERATURE_STATUS_THERMAL_ALERT);
}

int main(void) {
    test_disconnected_ignores_residual();
    test_connected_all_nan();
    test_coolant_nan_other_probes_known();
    test_coolant_sensor_fault();
    test_cold_threshold();
    test_coolant_danger_threshold();
    test_oil_danger_threshold();
    test_hot_beats_sensor_fault();
    puts("Temperature status tests: OK");
    return 0;
}
