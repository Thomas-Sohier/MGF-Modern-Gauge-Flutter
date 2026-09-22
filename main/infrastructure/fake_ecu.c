#include "infrastructure/fake_ecu.h"
#include "lvgl.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TICK_MS 40.0f
#define SWEEP_PERIOD 6.0f
#define RPM_IDLE 850.0f
#define RPM_PEAK 7800.0f

struct fake_ecu_s {
    ecu_data_t data;
    float phase;
    lv_timer_t *timer;
};

static void fake_ecu_timer_cb(lv_timer_t *timer) {
    fake_ecu_t *ecu = lv_timer_get_user_data(timer);
    const float dt = TICK_MS / 1000.0f;
    ecu->phase += dt * (2.0f * (float)M_PI / SWEEP_PERIOD);
    if (ecu->phase > 2.0f * (float)M_PI) ecu->phase -= 2.0f * (float)M_PI;

    const float k = 0.5f * (1.0f - cosf(ecu->phase));
    ecu->data.rpm = RPM_IDLE + k * (RPM_PEAK - RPM_IDLE);
    ecu->data.throttle = k * 100.0f;
    ecu->data.coolant_temp = 89.0f + 2.0f * sinf(ecu->phase * 0.3f);
    ecu->data.oil_temp = 96.0f + 3.0f * sinf(ecu->phase * 0.2f);
    ecu->data.battery_voltage = 14.2f - 0.15f * k;
    ecu->data.intake_air_temp = 31.0f + 5.0f * k;
    ecu->data.ambient_temp = 22.0f;
    ecu->data.fuel_rail_temp = 38.0f + 3.0f * k;
    ecu->data.map_sensor_kpa = 32.0f + 68.0f * k;
    ecu->data.throttle_pot_voltage = 0.48f + 4.05f * k;
    ecu->data.ignition_advance = 12.0f + 20.0f * k;
    ecu->data.ignition_advance_offset = -1.5f + 3.0f * k;
    ecu->data.coil_1_charge_time = 2.4f + 0.4f * k;
    ecu->data.coil_2_charge_time = 2.5f + 0.35f * k;
    ecu->data.coil_time_microseconds = 2450.0f + 400.0f * k;
    ecu->data.injector_1_pw = 2.1f + 8.0f * k;
    ecu->data.injector_2_pw = 2.2f + 7.9f * k;
    ecu->data.fuelling_feedback_percent = 98.0f + 4.0f * sinf(ecu->phase);
    ecu->data.short_term_trim_percent = 3.0f * sinf(ecu->phase * 1.3f);
    ecu->data.long_term_trim = -1.8f;
    ecu->data.lambda_mv = 450.0f + 350.0f * sinf(ecu->phase * 2.0f);
    ecu->data.o2_mv = ecu->data.lambda_mv;
    ecu->data.estimated_air_fuel = 14.7f + 0.8f * sinf(ecu->phase * 2.0f);
    ecu->data.lambda_sensor_duty_cycle = 50.0f + 25.0f * sinf(ecu->phase);
    ecu->data.idle_setpoint = 850.0f;
    ecu->data.idle_adjuster_rpm = 18.0f * sinf(ecu->phase);
    ecu->data.idle_error = ecu->data.rpm - ecu->data.idle_setpoint;
    ecu->data.idle_valve_position = 34.0f - 12.0f * k;
    ecu->data.idle_base_position = 30.0f;
}

fake_ecu_t *fake_ecu_create(void) {
    fake_ecu_t *ecu = calloc(1, sizeof(*ecu));
    if (ecu == NULL) return NULL;
    ecu->data = (ecu_data_t){
        .connected = true, .faults_available = true,
        .rpm = RPM_IDLE, .coolant_temp = 89.0f,
        .battery_voltage = 14.2f, .oil_temp = 96.0f,
        .ambient_temp = 22.0f, .intake_air_temp = 31.0f,
        .fuel_rail_temp = 38.0f, .map_sensor_kpa = 32.0f,
        .throttle_pot_voltage = 0.48f, .ignition_advance = 12.0f,
        .coil_1_charge_time = 2.4f, .coil_2_charge_time = 2.5f,
        .coil_time_microseconds = 2450.0f, .injector_1_pw = 2.1f,
        .injector_2_pw = 2.2f, .fuelling_feedback_percent = 98.0f,
        .long_term_trim = -1.8f, .lambda_mv = 450.0f, .o2_mv = 450.0f,
        .estimated_air_fuel = 14.7f, .lambda_sensor_duty_cycle = 50.0f,
        .idle_setpoint = RPM_IDLE, .idle_valve_position = 34.0f,
        .idle_base_position = 30.0f,
    };
    return ecu;
}

bool fake_ecu_start(fake_ecu_t *ecu) {
    if (ecu == NULL) return false;
    if (ecu->timer != NULL) return true;
    ecu->timer = lv_timer_create(fake_ecu_timer_cb, (uint32_t)TICK_MS, ecu);
    return ecu->timer != NULL;
}

void fake_ecu_stop(fake_ecu_t *ecu) {
    if (ecu == NULL || ecu->timer == NULL) return;
    lv_timer_delete(ecu->timer);
    ecu->timer = NULL;
}

void fake_ecu_destroy(fake_ecu_t *ecu) {
    if (ecu == NULL) return;
    fake_ecu_stop(ecu);
    free(ecu);
}

bool fake_ecu_read(const fake_ecu_t *ecu, ecu_data_t *out) {
    if (ecu == NULL || out == NULL) return false;
    *out = ecu->data;
    return true;
}

static bool fake_ecu_source_read(void *context, ecu_data_t *out) {
    return fake_ecu_read(context, out);
}

ecu_source_t fake_ecu_source(fake_ecu_t *ecu) {
    return (ecu_source_t){.read = fake_ecu_source_read, .context = ecu};
}
