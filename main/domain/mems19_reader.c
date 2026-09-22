#include "domain/mems19_reader.h"

#include <string.h>

#include "domain/mems_protocol.h"

void mems19_reader_init(mems19_reader_t *r, ecu_reader_t base,
                        kline_transport_t transport) {
    if (r == NULL) return;
    memset(r, 0, sizeof(*r));
    r->base = base;
    r->transport = transport;
}

static bool mems19_reader_connect(void *ctx) {
    mems19_reader_t *r = ctx;
    if (r == NULL) return false;
    // Réveil 5 bauds obligatoire avant le handshake standard.
    if (r->transport.wake_up == NULL) return false;
    if (r->transport.wake_up(r->transport.ctx, MEMS_ECU_ADDRESS) < 0)
        return false;
    return ecu_reader_connect(&r->base);
}

static bool mems19_reader_send_and_receive(void *ctx, uint8_t cmd, uint8_t *resp,
                                           size_t expected) {
    mems19_reader_t *r = ctx;
    if (r == NULL) return false;
    return ecu_reader_send_and_receive(&r->base, cmd, resp, expected);
}

static void mems19_reader_disconnect(void *ctx) {
    mems19_reader_t *r = ctx;
    if (r == NULL) return;
    ecu_reader_disconnect(&r->base);
}

ecu_reader_t mems19_reader_interface(mems19_reader_t *r) {
    return (ecu_reader_t){
        .connect = mems19_reader_connect,
        .send_and_receive = mems19_reader_send_and_receive,
        .disconnect = mems19_reader_disconnect,
        .ctx = r,
    };
}
