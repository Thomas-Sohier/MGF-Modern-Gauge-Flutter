#include "ecu_data.h"
#include "lvgl.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Source de données factice pour le test de faisabilité.
//
// Un balayage sinusoïdal fait monter/descendre le régime entre le ralenti et
// au-delà de la ligne rouge (~7000), ce qui permet de valider visuellement la
// coloration de la zone de danger et la fluidité du rendu segmenté. Le papillon
// suit le régime ; les autres canaux restent réalistes et stables.

#define TICK_MS      40.0f
#define SWEEP_PERIOD 6.0f   // s pour un aller-retour ralenti -> haut régime

#define RPM_IDLE     850.0f
#define RPM_PEAK     7800.0f

static ecu_data_t s_data = {
    .connected = true,
    .rpm = RPM_IDLE,
    .throttle = 0.0f,
    .coolant_temp = 89.0f,
    .battery_voltage = 14.2f,
    .oil_temp = 96.0f,
};

static float s_phase = 0.0f;

static void fake_ecu_timer_cb(lv_timer_t *t) {
    LV_UNUSED(t);
    const float dt = TICK_MS / 1000.0f;
    s_phase += dt * (2.0f * (float)M_PI / SWEEP_PERIOD);
    if (s_phase > 2.0f * (float)M_PI) s_phase -= 2.0f * (float)M_PI;

    // sin ∈ [-1,1] -> [0,1] : rampe douce ralenti -> pic -> ralenti.
    const float k = 0.5f * (1.0f - cosf(s_phase));
    s_data.rpm = RPM_IDLE + k * (RPM_PEAK - RPM_IDLE);
    s_data.throttle = k * 100.0f;

    // Dérives lentes plausibles.
    s_data.coolant_temp = 89.0f + 2.0f * sinf(s_phase * 0.3f);
    s_data.oil_temp = 96.0f + 3.0f * sinf(s_phase * 0.2f);
    s_data.battery_voltage = 14.2f - 0.15f * k; // chute légère sous charge
}

void fake_ecu_start(void) {
    lv_timer_create(fake_ecu_timer_cb, (uint32_t)TICK_MS, NULL);
}

const ecu_data_t *fake_ecu_current(void) {
    return &s_data;
}
