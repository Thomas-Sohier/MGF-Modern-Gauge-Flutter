#include "domain/mems_reader.h"

#include <string.h>

// Nombre de lectures infructueuses tolérées avant d'abandonner un paquet
// (calqué sur les 5 essais de `ecureader_mems.go::readSerial`).
#define READ_RETRY_MAX 5

void mems_reader_init(mems_reader_t *r, kline_transport_t transport,
                      uint32_t read_timeout_ms) {
    if (r == NULL) return;
    memset(r, 0, sizeof(*r));
    r->transport = transport;
    r->read_timeout_ms = read_timeout_ms;
    r->connected = false;
}

// Émet un octet de commande puis lit exactement `expected` octets de réponse.
// Vérifie que le premier octet reçu est l'écho de la commande.
static bool mems_reader_send_and_receive(void *ctx, uint8_t cmd, uint8_t *resp,
                                         size_t expected) {
    mems_reader_t *r = ctx;
    const kline_transport_t *t = &r->transport;
    if (t->write == NULL || t->read == NULL || resp == NULL || expected == 0)
        return false;

    if (t->write(t->ctx, &cmd, 1) != 1) return false;

    size_t got = 0;
    int retry = 0;
    while (got < expected) {
        int n = t->read(t->ctx, resp + got, expected - got, r->read_timeout_ms);
        if (n < 0) return false;  // erreur de transport
        if (n == 0) {             // timeout : on retente
            if (++retry >= READ_RETRY_MAX) return false;
            continue;
        }
        got += (size_t)n;
    }

    return resp[0] == cmd;  // l'ECU ré-émet la commande en tête de réponse
}

static bool mems_reader_connect(void *ctx) {
    mems_reader_t *r = ctx;
    r->connected = false;

    if (r->transport.flush != NULL) r->transport.flush(r->transport.ctx);

    uint8_t buf[MEMS_RESP_ECU_ID];
    if (!mems_reader_send_and_receive(r, MEMS_CMD_INIT_A, buf, MEMS_RESP_INIT_A))
        return false;
    if (!mems_reader_send_and_receive(r, MEMS_CMD_INIT_B, buf, MEMS_RESP_INIT_B))
        return false;
    if (!mems_reader_send_and_receive(r, MEMS_CMD_HEARTBEAT, buf,
                                      MEMS_RESP_HEARTBEAT))
        return false;
    if (!mems_reader_send_and_receive(r, MEMS_CMD_INIT_ECU_ID, buf,
                                      MEMS_RESP_ECU_ID))
        return false;

    memcpy(r->ecu_id, buf, MEMS_RESP_ECU_ID);
    r->connected = true;
    return true;
}

static void mems_reader_disconnect(void *ctx) {
    mems_reader_t *r = ctx;
    if (r != NULL) r->connected = false;
}

ecu_reader_t mems_reader_interface(mems_reader_t *r) {
    return (ecu_reader_t){
        .connect = mems_reader_connect,
        .send_and_receive = mems_reader_send_and_receive,
        .disconnect = mems_reader_disconnect,
        .ctx = r,
    };
}
