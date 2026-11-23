#include "domain/mems_session.h"

#include <math.h>
#include <string.h>

#include "domain/mems_mapping.h"

static void forget_snapshot(mems_session_t *s) {
    memset(&s->last, 0, sizeof(s->last));
    s->has_7d = false;
    s->cycles_since_7d = 0;
    s->coolant_spike_pending = false;
}

void mems_session_init(mems_session_t *s, ecu_reader_t reader) {
    if (s == NULL) return;
    memset(s, 0, sizeof(*s));
    s->reader = reader;
    s->connected = false;
    s->interval_7d = MEMS_SESSION_DEFAULT_7D_INTERVAL;
}

void mems_session_set_7d_interval(mems_session_t *s, uint8_t every) {
    if (s == NULL) return;
    s->interval_7d = every > 0 ? every : 1;
}

bool mems_session_connect(mems_session_t *s) {
    if (s == NULL) return false;
    s->connected = false;
    s->poll_failures = 0;
    // Ne jamais republier des champs 0x7D d'une session précédente.
    forget_snapshot(s);
    if (!ecu_reader_connect(&s->reader)) {
        ecu_reader_disconnect(&s->reader);
        return false;
    }
    s->connected = true;
    return true;
}

static bool poll_once(mems_session_t *s, mems_data_t *data, bool want_7d) {
    uint8_t f80[MEMS_FRAME_80_LEN];
    if (!ecu_reader_send_and_receive(&s->reader, MEMS_CMD_DATA_80, f80,
                                     MEMS_FRAME_80_LEN) ||
        !mems_parse_frame_80(f80, MEMS_FRAME_80_LEN, data)) {
        return false;
    }
    if (!want_7d) return true;
    uint8_t f7d[MEMS_FRAME_7D_LEN];
    return ecu_reader_send_and_receive(&s->reader, MEMS_CMD_DATA_7D, f7d,
                                       MEMS_FRAME_7D_LEN) &&
           mems_parse_frame_7d(f7d, MEMS_FRAME_7D_LEN, data);
}

// Filtre anti bit-flip sur la température d'eau. MEMS n'ayant pas de checksum,
// un octet `F80_COOLANT` corrompu peut faire franchir le seuil d'alerte sur une
// trame isolée. Un franchissement du seuil qui saute de plus de
// MEMS_SESSION_COOLANT_MAX_STEP_C en un cycle est donc retenu une trame : le
// cycle courant conserve l'ancienne valeur, et seule la trame suivante (qui
// reconfirme) publie la surchauffe. Une montée progressive, ou une session qui
// démarre déjà chaude, passent immédiatement.
static void apply_coolant_spike_filter(mems_session_t *s, mems_data_t *data) {
    const float prev = s->last.coolant_temp;
    const float next = data->coolant_temp;
    const bool implausible_jump =
        s->has_7d && isfinite(prev) && prev < MEMS_SESSION_COOLANT_ALERT_C &&
        isfinite(next) && next >= MEMS_SESSION_COOLANT_ALERT_C &&
        next - prev > MEMS_SESSION_COOLANT_MAX_STEP_C;
    if (!implausible_jump) {
        s->coolant_spike_pending = false;
        return;
    }
    if (s->coolant_spike_pending) {
        // 2e trame consécutive : la surchauffe est confirmée, on la publie.
        s->coolant_spike_pending = false;
        return;
    }
    s->coolant_spike_pending = true;
    data->coolant_temp = prev;
}

bool mems_session_poll(mems_session_t *s, ecu_data_t *out, mems_data_t *raw) {
    if (s == NULL || out == NULL || !s->connected) return false;

    const uint8_t every = s->interval_7d > 0 ? s->interval_7d : 1;
    const bool want_7d = !s->has_7d || s->cycles_since_7d + 1u >= every;

    // Décoder dans une copie : un échec ne doit altérer ni `out` ni l'état.
    mems_data_t data = s->last;
    if (!poll_once(s, &data, want_7d)) {
        if (++s->poll_failures >= MEMS_SESSION_MAX_POLL_FAILURES) {
            mems_session_disconnect(s);
        }
        return false;
    }
    s->poll_failures = 0;
    apply_coolant_spike_filter(s, &data);
    s->last = data;
    if (want_7d) {
        s->has_7d = true;
        s->cycles_since_7d = 0;
    } else {
        s->cycles_since_7d++;
    }

    mems_to_ecu_data(&data, true, out);
    if (raw != NULL) *raw = data;
    return true;
}

void mems_session_disconnect(mems_session_t *s) {
    if (s == NULL) return;
    s->connected = false;
    forget_snapshot(s);
    ecu_reader_disconnect(&s->reader);
}
