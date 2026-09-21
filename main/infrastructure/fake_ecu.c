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
}

fake_ecu_t *fake_ecu_create(void) {
    fake_ecu_t *ecu = calloc(1, sizeof(*ecu));
    if (ecu == NULL) return NULL;
    ecu->data = (ecu_data_t){
        .connected = true, .rpm = RPM_IDLE, .coolant_temp = 89.0f,
        .battery_voltage = 14.2f, .oil_temp = 96.0f,
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
