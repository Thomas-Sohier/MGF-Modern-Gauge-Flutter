#include "domain/mems_session.h"

#include <string.h>

void mems_session_init(mems_session_t *s, ecu_reader_t reader) {
    if (s == NULL) return;
    memset(s, 0, sizeof(*s));
    s->reader = reader;
    s->connected = false;
}

bool mems_session_connect(mems_session_t *s) {
    if (s == NULL) return false;
    s->connected = false;
    s->poll_failures = 0;
    if (!ecu_reader_connect(&s->reader)) {
        ecu_reader_disconnect(&s->reader);
        return false;
    }
    s->connected = true;
    return true;
}

bool mems_session_poll(mems_session_t *s, ecu_data_t *out, mems_data_t *raw) {
    if (s == NULL || out == NULL || !s->connected) return false;

    uint8_t f80[MEMS_FRAME_80_LEN];
    uint8_t f7d[MEMS_FRAME_7D_LEN];
    mems_data_t data;
    memset(&data, 0, sizeof(data));

    if (!ecu_reader_send_and_receive(&s->reader, MEMS_CMD_DATA_80, f80,
                                     MEMS_FRAME_80_LEN) ||
        !mems_parse_frame_80(f80, MEMS_FRAME_80_LEN, &data) ||
        !ecu_reader_send_and_receive(&s->reader, MEMS_CMD_DATA_7D, f7d,
                                     MEMS_FRAME_7D_LEN) ||
        !mems_parse_frame_7d(f7d, MEMS_FRAME_7D_LEN, &data)) {
        if (++s->poll_failures >= MEMS_SESSION_MAX_POLL_FAILURES) {
            mems_session_disconnect(s);
        }
        return false;
    }
    s->poll_failures = 0;

    mems_to_ecu_data(&data, true, out);
    if (raw != NULL) *raw = data;
    return true;
}

void mems_session_disconnect(mems_session_t *s) {
    if (s == NULL) return;
    s->connected = false;
    ecu_reader_disconnect(&s->reader);
}
