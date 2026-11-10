#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "domain/mems19_reader.h"
#include "domain/mems_mapping.h"
#include "domain/mems_protocol.h"
#include "domain/mems_reader.h"
#include "domain/mems_session.h"

// Trames de référence extraites du projet Go `andrewdjackson/rosco`
// (ecureader.go, responseMap "80"/"7D") — sert d'oracle de décodage.
static const uint8_t FRAME_80[MEMS_FRAME_80_LEN] = {
    0x80, 0x1c, 0x04, 0xa5, 0x4b, 0xff, 0x4c, 0xff, 0x31, 0x82,
    0x22, 0x00, 0x20, 0x01, 0x00, 0x00, 0x00, 0x20, 0x84, 0x78,
    0x00, 0x1d, 0x00, 0x44, 0x06, 0x59, 0x10, 0x00, 0x00,
};
static const uint8_t FRAME_7D[MEMS_FRAME_7D_LEN] = {
    0x7d, 0x20, 0x10, 0x14, 0xff, 0x92, 0x40, 0x57, 0xff, 0xff, 0x01,
    0x00, 0x80, 0x64, 0x00, 0xff, 0x64, 0xff, 0xff, 0x30, 0x80, 0x80,
    0x0e, 0xff, 0x16, 0x80, 0x1b, 0x00, 0x22, 0x00, 0x31, 0xc0, 0x1f,
};

static void check(bool cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        _Exit(1);
    }
}

static bool near(float a, float b) {
    return fabsf(a - b) < 1e-3f;
}

static void test_parse_frame_80(void) {
    mems_data_t d;
    memset(&d, 0, sizeof(d));
    check(mems_parse_frame_80(FRAME_80, MEMS_FRAME_80_LEN, &d), "80 parses");
    check(d.engine_rpm == 1189, "rpm = 0x04a5");
    check(near(d.coolant_temp, 20.0f), "coolant = 75 - 55");
    check(isnan(d.ambient_temp), "ambient raw 0xFF = unsupported (NAN)");
    check(isnan(d.fuel_temp), "fuel raw 0xFF = unsupported (NAN)");
    check(near(d.intake_air_temp, 21.0f), "intake = 76 - 55");
    check(near(d.battery_voltage, 13.0f), "battery = 130 / 10");
    check(near(d.throttle_pot_voltage, 0.68f), "throttle pot = 34 * 0.02");
    check(d.iac_position == 120, "iac position = 0x78");
    check(near(d.ignition_advance, 10.0f), "ign advance = 68/2 - 24");
    check(near(d.coil_time, 3.25f), "coil time = 0x0659 * 0.002");
    check(!d.coolant_sensor_fault && !d.throttle_pot_fault, "no dtc faults");
}

static void test_parse_frame_7d(void) {
    mems_data_t d;
    memset(&d, 0, sizeof(d));
    check(mems_parse_frame_7d(FRAME_7D, MEMS_FRAME_7D_LEN, &d), "7d parses");
    check(d.ignition_switch, "ignition switch on");
    check(d.throttle_angle == 12, "throttle angle = round(20 * 6/10)");
    check(near(d.air_fuel_ratio, 14.6f), "afr = 146 / 10");
    check(d.lambda_voltage_mv == 435, "lambda = 87 * 5");
}

static void test_parse_rejects_bad_input(void) {
    mems_data_t d;
    check(!mems_parse_frame_80(FRAME_80, MEMS_FRAME_80_LEN - 1, &d),
          "80 wrong size rejected");
    uint8_t bad[MEMS_FRAME_80_LEN];
    memcpy(bad, FRAME_80, sizeof(bad));
    bad[0] = 0x7d; // mauvais écho de commande
    check(!mems_parse_frame_80(bad, MEMS_FRAME_80_LEN, &d),
          "80 wrong echo rejected");
    check(!mems_parse_frame_7d(FRAME_7D, 4, &d), "7d wrong size rejected");

    memcpy(bad, FRAME_80, sizeof(bad));
    bad[1] = 0x1B;
    check(!mems_parse_frame_80(bad, MEMS_FRAME_80_LEN, &d),
          "80 wrong size byte rejected");
    uint8_t bad7d[MEMS_FRAME_7D_LEN];
    memcpy(bad7d, FRAME_7D, sizeof(bad7d));
    bad7d[1] = 0x1C;
    check(!mems_parse_frame_7d(bad7d, MEMS_FRAME_7D_LEN, &d),
          "7d wrong size byte rejected");
}

// Écho local du transceiver non absorbé : toute la trame est décalée d'un
// octet (« 80 80 1C 04 … »). L'écho [0] est correct, seul l'octet de taille
// permet de rejeter la trame.
static void test_parse_rejects_shifted_frames(void) {
    mems_data_t d;
    memset(&d, 0, sizeof(d));
    d.engine_rpm = 777;
    uint8_t shifted[MEMS_FRAME_80_LEN];
    shifted[0] = MEMS_CMD_DATA_80;
    memcpy(shifted + 1, FRAME_80, MEMS_FRAME_80_LEN - 1);
    check(!mems_parse_frame_80(shifted, MEMS_FRAME_80_LEN, &d),
          "80 shifted by local echo rejected");
    check(d.engine_rpm == 777, "rejected 80 leaves output untouched");

    uint8_t shifted7d[MEMS_FRAME_7D_LEN];
    shifted7d[0] = MEMS_CMD_DATA_7D;
    memcpy(shifted7d + 1, FRAME_7D, MEMS_FRAME_7D_LEN - 1);
    check(!mems_parse_frame_7d(shifted7d, MEMS_FRAME_7D_LEN, &d),
          "7d shifted by local echo rejected");
}

static void test_parse_rpm_plausibility(void) {
    mems_data_t d;
    uint8_t f[MEMS_FRAME_80_LEN];
    memcpy(f, FRAME_80, sizeof(f));
    f[2] = 0x00;
    f[3] = 0x00;
    check(mems_parse_frame_80(f, MEMS_FRAME_80_LEN, &d) && d.engine_rpm == 0,
          "engine stopped (0 rpm) is valid");
    f[2] = (uint8_t)(MEMS_RPM_MAX_PLAUSIBLE >> 8);
    f[3] = (uint8_t)(MEMS_RPM_MAX_PLAUSIBLE & 0xFF);
    check(mems_parse_frame_80(f, MEMS_FRAME_80_LEN, &d) &&
              d.engine_rpm == MEMS_RPM_MAX_PLAUSIBLE,
          "rpm at plausibility bound accepted");
    f[3]++;
    check(!mems_parse_frame_80(f, MEMS_FRAME_80_LEN, &d),
          "rpm above plausibility bound rejected");
    f[2] = 0xFF;
    f[3] = 0xFF;
    check(!mems_parse_frame_80(f, MEMS_FRAME_80_LEN, &d),
          "rpm 0xFFFF rejected");
}

static void test_unsupported_temps_map_to_nan(void) {
    uint8_t f[MEMS_FRAME_80_LEN];
    memcpy(f, FRAME_80, sizeof(f));
    f[4] = 0xFF; // eau
    f[6] = 0xFF; // air d'admission
    mems_data_t d;
    memset(&d, 0, sizeof(d));
    check(mems_parse_frame_80(f, MEMS_FRAME_80_LEN, &d), "80 with 0xFF temps");
    check(mems_parse_frame_7d(FRAME_7D, MEMS_FRAME_7D_LEN, &d), "7d parses");
    ecu_data_t e;
    mems_to_ecu_data(&d, true, &e);
    check(isnan(e.coolant_temp), "coolant 0xFF -> NAN, never 200 C");
    check(isnan(e.intake_air_temp), "intake 0xFF -> NAN");
    check(isnan(e.ambient_temp), "ambient 0xFF -> NAN");
    check(isnan(e.fuel_rail_temp), "fuel 0xFF -> NAN");
    check(near(e.rpm, 1189.0f), "rpm still decoded");

    f[4] = 0xFE; // 199 °C : valeur brute légitime, non sentinelle
    check(mems_parse_frame_80(f, MEMS_FRAME_80_LEN, &d) &&
              near(d.coolant_temp, 199.0f),
          "0xFE is a real temperature");
}

static void test_to_ecu_data(void) {
    mems_data_t d;
    memset(&d, 0, sizeof(d));
    mems_parse_frame_80(FRAME_80, MEMS_FRAME_80_LEN, &d);
    mems_parse_frame_7d(FRAME_7D, MEMS_FRAME_7D_LEN, &d);

    ecu_data_t e;
    memset(&e, 0xA5, sizeof(e)); // garbage: every field must be written
    mems_to_ecu_data(&d, true, &e);
    check(e.connected, "connected propagated");
    check(near(e.rpm, 1189.0f), "ecu rpm");
    check(near(e.coolant_temp, 20.0f), "ecu coolant");
    check(isnan(e.ambient_temp) && isnan(e.fuel_rail_temp),
          "unsupported temps stay unavailable");
    check(near(e.battery_voltage, 13.0f), "ecu battery");
    check(near(e.throttle, 2.0f), "throttle % from pot (0.68-0.6)/4*100");
    check(isnan(e.oil_temp), "oil temp unavailable (no MEMS sensor)");
    check(isnan(e.injector_1_pw), "unmapped injector pw is unavailable");
    check(isnan(e.idle_setpoint), "unmapped idle setpoint is unavailable");
    check(isnan(e.lambda_sensor_duty_cycle),
          "unmapped heater duty is unavailable");
    check(e.faults_available, "MEMS reports fault state");
    check(e.fault_flags == 0, "reference frame has no fault");

    d.coolant_sensor_fault = true;
    d.throttle_pot_fault = true;
    mems_to_ecu_data(&d, true, &e);
    check(e.fault_flags == (ECU_FAULT_COOLANT_SENSOR | ECU_FAULT_THROTTLE_POT),
          "MEMS DTC bits map to fault flags");
}

static void test_unavailable_snapshot(void) {
    const ecu_data_t e = ecu_data_unavailable();
    check(!e.connected, "unavailable snapshot is disconnected");
    check(!e.faults_available, "unavailable snapshot has unknown faults");
    const float *field = &e.rpm;
    const size_t count =
        (sizeof(e) - ((const char *)field - (const char *)&e)) / sizeof(float);
    for (size_t i = 0; i < count; i++) {
        check(isnan(field[i]), "every unavailable measurement is NAN");
    }
}

// --- Transport factice piloté par script pour tester la session -------------
typedef struct {
    uint8_t out[64]; // octets à renvoyer en lecture
    size_t out_len;
    size_t out_pos;
    uint8_t last_cmd;
    int connect_fail_echo; // si !=0, corrompt l'écho du prochain write
    int silent;            // ECU absente : aucune réponse
    int drop_data;         // abandonne les réponses de polling
    int read_chunk_limit;  // force les lectures partielles
    int read_timeouts;     // nombre de timeouts simulés avant lecture
    uint32_t last_timeout;
    int wake_calls; // nombre d'appels au réveil 5 bauds
    int wake_fail;
    uint8_t wake_addr;       // adresse ECU reçue au dernier réveil
    int n80, n7d;            // nombre de requêtes 0x80 / 0x7D reçues
    int drop_7d;             // abandonne seulement les réponses 0x7D
    const uint8_t *frame_80; // trames de réponse (NULL => référence)
    const uint8_t *frame_7d;
} mock_transport_t;

static void mock_enqueue(mock_transport_t *m, const uint8_t *b, size_t n) {
    if (m->out_pos == m->out_len) m->out_pos = m->out_len = 0;
    check(n <= sizeof(m->out) - m->out_len, "mock response queue capacity");
    memcpy(m->out + m->out_len, b, n);
    m->out_len += n;
}

static int mock_write(void *ctx, const uint8_t *buf, size_t len) {
    mock_transport_t *m = ctx;
    if (m->silent) return (int)len;
    m->last_cmd = buf[len - 1];
    uint8_t echo = m->last_cmd;
    if (m->connect_fail_echo) {
        echo ^= 0xFF; // écho volontairement faux
        m->connect_fail_echo = 0;
    }
    if (m->drop_data &&
        (m->last_cmd == MEMS_CMD_DATA_80 || m->last_cmd == MEMS_CMD_DATA_7D))
        return (int)len;

    switch (m->last_cmd) {
    case MEMS_CMD_INIT_A:
    case MEMS_CMD_INIT_B:
        mock_enqueue(m, &echo, 1);
        break;
    case MEMS_CMD_HEARTBEAT: {
        uint8_t r[2] = {echo, 0x00};
        mock_enqueue(m, r, 2);
        break;
    }
    case MEMS_CMD_INIT_ECU_ID: {
        uint8_t r[5] = {echo, 0x99, 0x00, 0x02, 0x03};
        mock_enqueue(m, r, 5);
        break;
    }
    case MEMS_CMD_DATA_80:
        m->n80++;
        mock_enqueue(m, m->frame_80 ? m->frame_80 : FRAME_80,
                     MEMS_FRAME_80_LEN);
        break;
    case MEMS_CMD_DATA_7D:
        m->n7d++;
        if (m->drop_7d) break;
        mock_enqueue(m, m->frame_7d ? m->frame_7d : FRAME_7D,
                     MEMS_FRAME_7D_LEN);
        break;
    default:
        break;
    }
    return (int)len;
}

static int mock_read(void *ctx, uint8_t *buf, size_t len, uint32_t timeout_ms) {
    mock_transport_t *m = ctx;
    m->last_timeout = timeout_ms;
    if (m->read_timeouts > 0) {
        m->read_timeouts--;
        return 0;
    }
    size_t avail = m->out_len - m->out_pos;
    if (avail == 0) return 0;
    size_t n = avail < len ? avail : len;
    if (m->read_chunk_limit > 0 && n > (size_t)m->read_chunk_limit)
        n = (size_t)m->read_chunk_limit;
    memcpy(buf, m->out + m->out_pos, n);
    m->out_pos += n;
    return (int)n;
}

static void mock_flush(void *ctx) {
    mock_transport_t *m = ctx;
    m->out_len = m->out_pos = 0;
}

static int mock_wake_up(void *ctx, uint8_t ecu_address) {
    mock_transport_t *m = ctx;
    m->wake_calls++;
    m->wake_addr = ecu_address;
    return m->wake_fail ? -1 : 0;
}

static void test_session_connect_and_poll(void) {
    mock_transport_t mock;
    memset(&mock, 0, sizeof(mock));
    kline_transport_t t = {.write = mock_write,
                           .read = mock_read,
                           .flush = mock_flush,
                           .ctx = &mock};

    mems_reader_t base;
    mems_reader_init(&base, t, 100);
    mems_session_t s;
    mems_session_init(&s, mems_reader_interface(&base));
    check(mems_session_connect(&s), "handshake succeeds");
    check(mems_session_is_connected(&s), "session marked connected");
    check(mems_reader_ecu_id(&base)[0] == MEMS_CMD_INIT_ECU_ID,
          "ecu id echo stored");

    ecu_data_t e;
    mems_data_t raw;
    check(mems_session_poll(&s, &e, &raw), "poll succeeds");
    check(raw.engine_rpm == 1189, "poll decoded rpm");
    check(near(e.battery_voltage, 13.0f), "poll decoded battery");
}

static void test_session_connect_failure(void) {
    mock_transport_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.connect_fail_echo = 1; // premier écho corrompu
    kline_transport_t t = {.write = mock_write,
                           .read = mock_read,
                           .flush = mock_flush,
                           .ctx = &mock};

    mems_reader_t base;
    mems_reader_init(&base, t, 100);
    mems_session_t s;
    mems_session_init(&s, mems_reader_interface(&base));
    check(!mems_session_connect(&s), "handshake fails on bad echo");
    check(!s.connected, "session not connected after failure");
    check(!base.connected, "reader disconnected after failed handshake");
}

static void test_session_tolerates_partial_reads_and_timeouts(void) {
    mock_transport_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.read_chunk_limit = 1;
    mock.read_timeouts = 4;
    kline_transport_t t = {.write = mock_write,
                           .read = mock_read,
                           .flush = mock_flush,
                           .ctx = &mock};

    mems_reader_t base;
    mems_reader_init(&base, t, 0); // uses the safe 100 ms default
    mems_session_t s;
    mems_session_init(&s, mems_reader_interface(&base));
    check(mems_session_connect(&s), "partial/temporary timeout handshake");
    check(mock.last_timeout == 100, "default read timeout is applied");

    ecu_data_t e;
    check(mems_session_poll(&s, &e, NULL), "partial reads poll");
}

static void test_absent_ecu_and_poll_timeout_are_disconnected(void) {
    mock_transport_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.silent = 1;
    kline_transport_t t = {.write = mock_write,
                           .read = mock_read,
                           .flush = mock_flush,
                           .ctx = &mock};

    mems_reader_t base;
    mems_reader_init(&base, t, 1);
    mems_session_t s;
    mems_session_init(&s, mems_reader_interface(&base));
    check(!mems_session_connect(&s), "absent ECU times out");
    check(!s.connected, "absent ECU remains disconnected");

    memset(&mock, 0, sizeof(mock));
    mems_reader_init(&base, t, 100);
    mems_session_init(&s, mems_reader_interface(&base));
    check(mems_session_connect(&s), "reconnect before poll timeout");
    mock.drop_data = 1;
    ecu_data_t unchanged = {.rpm = 1234.0f, .connected = true};
    for (int i = 1; i < MEMS_SESSION_MAX_POLL_FAILURES; i++) {
        check(!mems_session_poll(&s, &unchanged, NULL), "poll timeout fails");
        check(s.connected, "isolated poll timeouts keep the link");
    }
    check(!mems_session_poll(&s, &unchanged, NULL), "poll timeout disconnects");
    check(!s.connected && !base.connected,
          "repeated poll timeouts clear connection");
    check(unchanged.rpm == 1234.0f && unchanged.connected,
          "failed poll leaves output untouched");
}

static void test_poll_success_resets_failure_count(void) {
    mock_transport_t mock;
    memset(&mock, 0, sizeof(mock));
    kline_transport_t t = {.write = mock_write,
                           .read = mock_read,
                           .flush = mock_flush,
                           .ctx = &mock};

    mems_reader_t base;
    mems_reader_init(&base, t, 1);
    mems_session_t s;
    mems_session_init(&s, mems_reader_interface(&base));
    check(mems_session_connect(&s), "connect for failure reset");

    ecu_data_t e;
    for (int round = 0; round < 3; round++) {
        mock.drop_data = 1;
        for (int i = 1; i < MEMS_SESSION_MAX_POLL_FAILURES; i++) {
            check(!mems_session_poll(&s, &e, NULL), "dropped frame fails");
        }
        mock.drop_data = 0;
        check(mems_session_poll(&s, &e, NULL), "recovered poll succeeds");
        check(mems_session_is_connected(&s) &&
                  mems_session_poll_failures(&s) == 0,
              "success resets failures");
    }
}

static void test_late_bytes_are_flushed_before_command(void) {
    mock_transport_t mock;
    memset(&mock, 0, sizeof(mock));
    kline_transport_t t = {.write = mock_write,
                           .read = mock_read,
                           .flush = mock_flush,
                           .ctx = &mock};

    mems_reader_t base;
    mems_reader_init(&base, t, 1);
    mems_session_t s;
    mems_session_init(&s, mems_reader_interface(&base));
    check(mems_session_connect(&s), "connect for late bytes");

    const uint8_t late[3] = {0x7d, 0x20, 0x10}; // fin d'une réponse tardive
    mock_enqueue(&mock, late, sizeof(late));
    ecu_data_t e;
    check(mems_session_poll(&s, &e, NULL), "late bytes do not desync poll");
    check(near(e.rpm, 1189.0f), "poll decodes the fresh frame");
}

static void test_session_1_9_wakes_before_handshake(void) {
    mock_transport_t mock;
    memset(&mock, 0, sizeof(mock));
    kline_transport_t t = {.write = mock_write,
                           .read = mock_read,
                           .flush = mock_flush,
                           .wake_up = mock_wake_up,
                           .ctx = &mock};

    mems_reader_t base;
    mems_reader_init(&base, t, 100);
    mems19_reader_t r19;
    mems19_reader_init(&r19, mems_reader_interface(&base), t);
    mems_session_t s;
    mems_session_init(&s, mems19_reader_interface(&r19));
    check(mems_session_connect(&s), "1.9 handshake succeeds after wake-up");
    check(mock.wake_calls == 1, "1.9 performs slow init exactly once");
    check(mock.wake_addr == MEMS_ECU_ADDRESS, "1.9 wakes ECU address 0x16");

    ecu_data_t e;
    check(mems_session_poll(&s, &e, NULL), "1.9 poll delegates to base reader");
    check(near(e.battery_voltage, 13.0f), "1.9 decoded battery");
}

static void test_session_1_9_wakeup_failure(void) {
    mock_transport_t mock;
    memset(&mock, 0, sizeof(mock));
    mock.wake_fail = 1;
    kline_transport_t t = {.write = mock_write,
                           .read = mock_read,
                           .flush = mock_flush,
                           .wake_up = mock_wake_up,
                           .ctx = &mock};

    mems_reader_t base;
    mems_reader_init(&base, t, 100);
    mems19_reader_t r19;
    mems19_reader_init(&r19, mems_reader_interface(&base), t);
    mems_session_t s;
    mems_session_init(&s, mems19_reader_interface(&r19));
    check(!mems_session_connect(&s), "1.9 fails when slow init fails");
    check(mock.wake_calls == 1, "failed 1.9 slow init called once");
}

static void test_session_1_9_requires_wakeup(void) {
    mock_transport_t mock;
    memset(&mock, 0, sizeof(mock));
    // transport sans wake_up : la 1.9 doit échouer proprement.
    kline_transport_t t = {.write = mock_write,
                           .read = mock_read,
                           .flush = mock_flush,
                           .wake_up = NULL,
                           .ctx = &mock};

    mems_reader_t base;
    mems_reader_init(&base, t, 100);
    mems19_reader_t r19;
    mems19_reader_init(&r19, mems_reader_interface(&base), t);
    mems_session_t s;
    mems_session_init(&s, mems19_reader_interface(&r19));
    check(!mems_session_connect(&s), "1.9 fails without wake_up support");
}

static void session_setup(mock_transport_t *mock, mems_reader_t *base,
                          mems_session_t *s) {
    memset(mock, 0, sizeof(*mock));
    kline_transport_t t = {.write = mock_write,
                           .read = mock_read,
                           .flush = mock_flush,
                           .ctx = mock};
    mems_reader_init(base, t, 1);
    mems_session_init(s, mems_reader_interface(base));
}

static void test_poll_7d_one_cycle_in_n(void) {
    mock_transport_t mock;
    mems_reader_t base;
    mems_session_t s;
    session_setup(&mock, &base, &s);
    check(s.interval_7d == MEMS_SESSION_DEFAULT_7D_INTERVAL &&
              MEMS_SESSION_DEFAULT_7D_INTERVAL == 3,
          "default 0x7D interval is 1 cycle in 3");
    check(mems_session_connect(&s), "connect for cadence");

    ecu_data_t e;
    mems_data_t raw;
    for (int cycle = 0; cycle < 9; cycle++) {
        check(mems_session_poll(&s, &e, &raw), "cadence poll succeeds");
        check(near(e.estimated_air_fuel, 14.6f) && raw.lambda_voltage_mv == 435,
              "0x7D fields keep their last value between reads");
    }
    check(mock.n80 == 9, "0x80 polled every cycle");
    check(mock.n7d == 3, "0x7D polled one cycle in 3");

    mems_session_set_7d_interval(&s, 0);
    mock.n7d = 0;
    for (int cycle = 0; cycle < 4; cycle++)
        check(mems_session_poll(&s, &e, NULL), "interval 1 poll");
    check(mock.n7d == 4, "interval 0/1 polls 0x7D every cycle");
}

static void test_poll_7d_failure_counts_and_stays_due(void) {
    mock_transport_t mock;
    mems_reader_t base;
    mems_session_t s;
    session_setup(&mock, &base, &s);
    check(mems_session_connect(&s), "connect for 7d failure");
    ecu_data_t e;
    check(mems_session_poll(&s, &e, NULL), "first poll reads both frames");
    check(mems_session_poll(&s, &e, NULL), "80-only poll 1");
    check(mems_session_poll(&s, &e, NULL), "80-only poll 2");
    check(mock.n7d == 1, "no 0x7D yet");

    mock.drop_7d = 1;
    ecu_data_t keep = {.rpm = 42.0f};
    check(!mems_session_poll(&s, &keep, NULL), "lost 0x7D is a lost frame");
    check(mems_session_poll_failures(&s) == 1, "0x7D loss counted");
    check(keep.rpm == 42.0f, "failed poll leaves output untouched");
    mock.drop_7d = 0;
    const int before = mock.n7d;
    check(mems_session_poll(&s, &e, NULL), "recovered poll");
    check(mock.n7d == before + 1, "due 0x7D retried on the next cycle");
    check(mems_session_poll_failures(&s) == 0, "success resets failures");
}

static void test_reconnect_never_reuses_old_7d(void) {
    mock_transport_t mock;
    mems_reader_t base;
    mems_session_t s;
    session_setup(&mock, &base, &s);
    check(mems_session_connect(&s), "first connection");
    ecu_data_t e;
    check(mems_session_poll(&s, &e, NULL), "poll with reference 0x7D");
    check(near(e.estimated_air_fuel, 14.6f), "reference AFR");

    // Perte de liaison : 0x80 muette jusqu'à la déconnexion.
    mock.drop_data = 1;
    for (int i = 0; i < MEMS_SESSION_MAX_POLL_FAILURES; i++)
        check(!mems_session_poll(&s, &e, NULL), "lost frame");
    check(!mems_session_is_connected(&s), "session disconnected");

    // Nouvelle connexion avec une 0x7D différente : le tout premier poll doit
    // l'interroger, et ne jamais publier l'ancienne valeur.
    uint8_t f7d[MEMS_FRAME_7D_LEN];
    memcpy(f7d, FRAME_7D, sizeof(f7d));
    f7d[5] = 120; // AFR 12.0
    mock.drop_data = 0;
    mock.frame_7d = f7d;
    mock.n7d = 0;
    check(mems_session_connect(&s), "reconnection");
    mock.drop_7d = 1;
    check(!mems_session_poll(&s, &e, NULL),
          "first poll after reconnect needs a fresh 0x7D");
    mock.drop_7d = 0;
    check(mems_session_poll(&s, &e, NULL), "fresh 0x7D received");
    check(near(e.estimated_air_fuel, 12.0f), "new session AFR, not stale one");
    check(mock.n7d == 2, "0x7D requested on both first polls");
}

static void test_session_rejects_shifted_frame(void) {
    mock_transport_t mock;
    mems_reader_t base;
    mems_session_t s;
    session_setup(&mock, &base, &s);
    check(mems_session_connect(&s), "connect for shifted frame");
    uint8_t shifted[MEMS_FRAME_80_LEN];
    shifted[0] = MEMS_CMD_DATA_80;
    memcpy(shifted + 1, FRAME_80, MEMS_FRAME_80_LEN - 1);
    mock.frame_80 = shifted;
    ecu_data_t e;
    check(!mems_session_poll(&s, &e, NULL), "shifted 0x80 is a lost frame");
    check(mems_session_poll_failures(&s) == 1, "shifted frame counted");
}

int main(void) {
    test_parse_frame_80();
    test_parse_frame_7d();
    test_parse_rejects_bad_input();
    test_parse_rejects_shifted_frames();
    test_parse_rpm_plausibility();
    test_unsupported_temps_map_to_nan();
    test_to_ecu_data();
    test_unavailable_snapshot();
    test_session_connect_and_poll();
    test_session_connect_failure();
    test_session_tolerates_partial_reads_and_timeouts();
    test_absent_ecu_and_poll_timeout_are_disconnected();
    test_poll_success_resets_failure_count();
    test_late_bytes_are_flushed_before_command();
    test_session_1_9_wakes_before_handshake();
    test_session_1_9_wakeup_failure();
    test_session_1_9_requires_wakeup();
    test_poll_7d_one_cycle_in_n();
    test_poll_7d_failure_counts_and_stays_due();
    test_reconnect_never_reuses_old_7d();
    test_session_rejects_shifted_frame();
    puts("mems tests: OK");
    return 0;
}
