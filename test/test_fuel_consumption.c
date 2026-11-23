#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "domain/fuel_consumption.h"

static void check(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        _Exit(1);
    }
}

static bool near(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

static void test_unavailable_inputs(void) {
    check(isnan(fuel_consumption_lph(NULL)), "NULL data -> NAN");

    ecu_data_t d = ecu_data_unavailable();
    check(isnan(fuel_consumption_lph(&d)), "disconnected snapshot -> NAN");

    d.connected = true;
    d.rpm = 875.0f;
    d.map_sensor_kpa = 34.0f;
    d.connected = false;
    check(isnan(fuel_consumption_lph(&d)),
          "disconnected with residual values -> NAN");

    d.connected = true;
    d.rpm = NAN;
    check(isnan(fuel_consumption_lph(&d)), "missing RPM -> NAN");

    d.rpm = -10.0f;
    check(isnan(fuel_consumption_lph(&d)), "negative RPM -> NAN");

    d.rpm = 875.0f;
    d.map_sensor_kpa = NAN;
    check(isnan(fuel_consumption_lph(&d)), "missing MAP -> NAN");

    d.map_sensor_kpa = 0.0f;
    check(isnan(fuel_consumption_lph(&d)), "zero MAP -> NAN");

    d.map_sensor_kpa = -5.0f;
    check(isnan(fuel_consumption_lph(&d)), "negative MAP -> NAN");
}

static void test_engine_stopped(void) {
    ecu_data_t d = ecu_data_unavailable();
    d.connected = true;
    d.rpm = 0.0f;
    d.map_sensor_kpa = 100.0f;
    check(near(fuel_consumption_lph(&d), 0.0f, 1e-6f),
          "engine stopped (0 RPM) -> 0.0 L/h");
}

static void test_nominal_operating_points(void) {
    // Ralenti chaud MGF 1.8i (875 tr/min, 34 kPa, IAT 31 °C, AFR 14.65)
    ecu_data_t idle = ecu_data_unavailable();
    idle.connected = true;
    idle.rpm = 875.0f;
    idle.map_sensor_kpa = 34.0f;
    idle.intake_air_temp = 31.0f;
    idle.estimated_air_fuel = 14.65f;
    const float idle_lph = fuel_consumption_lph(&idle);
    check(near(idle_lph, 1.43f, 0.05f), "warm idle is ~1.43 L/h");

    // Croisière stabilisée (2800 tr/min, 55 kPa, IAT 25 °C, AFR 14.7)
    ecu_data_t cruise = idle;
    cruise.rpm = 2800.0f;
    cruise.map_sensor_kpa = 55.0f;
    cruise.intake_air_temp = 25.0f;
    cruise.estimated_air_fuel = 14.7f;
    const float cruise_lph = fuel_consumption_lph(&cruise);
    check(near(cruise_lph, 7.53f, 0.1f), "cruise is ~7.5 L/h");
    check(cruise_lph > idle_lph, "cruise > idle consumption");

    // Pleine charge (6500 tr/min, 100 kPa, IAT 35 °C, AFR 12.8)
    ecu_data_t wot = idle;
    wot.rpm = 6500.0f;
    wot.map_sensor_kpa = 100.0f;
    wot.intake_air_temp = 35.0f;
    wot.estimated_air_fuel = 12.8f;
    const float wot_lph = fuel_consumption_lph(&wot);
    check(near(wot_lph, 35.2f, 0.5f), "full load 6500 RPM is ~35 L/h");
    check(wot_lph > cruise_lph, "WOT > cruise consumption");
}

static void test_fallbacks_for_missing_iat_and_afr(void) {
    ecu_data_t baseline = ecu_data_unavailable();
    baseline.connected = true;
    baseline.rpm = 2000.0f;
    baseline.map_sensor_kpa = 50.0f;
    baseline.intake_air_temp = FUEL_DEFAULT_IAT_C;
    baseline.estimated_air_fuel = FUEL_DEFAULT_AFR;
    const float expected = fuel_consumption_lph(&baseline);

    // IAT et AFR à NAN (ex. sonde IAT absente, trame 0x7D pas encore reçue)
    ecu_data_t missing = baseline;
    missing.intake_air_temp = NAN;
    missing.estimated_air_fuel = NAN;
    check(near(fuel_consumption_lph(&missing), expected, 1e-5f),
          "NAN IAT and AFR fall back to 25 C and 14.7 AFR");

    // Valeurs hors bornes physiques (ex. AFR = 0 avant 0x7D)
    ecu_data_t out_of_range = baseline;
    out_of_range.intake_air_temp = 250.0f;
    out_of_range.estimated_air_fuel = 0.0f;
    check(near(fuel_consumption_lph(&out_of_range), expected, 1e-5f),
          "out-of-range IAT and AFR fall back to defaults");
}

int main(void) {
    test_unavailable_inputs();
    test_engine_stopped();
    test_nominal_operating_points();
    test_fallbacks_for_missing_iat_and_afr();
    puts("fuel_consumption tests: OK");
    return 0;
}
