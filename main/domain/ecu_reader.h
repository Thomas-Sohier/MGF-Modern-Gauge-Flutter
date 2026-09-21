#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Interface bas niveau d'un lecteur ECU — calquée sur `ECUReader` du projet Go
// de référence (`andrewdjackson/rosco`). Elle isole le *dialogue* (établir la
// liaison, échanger une commande/réponse, se déconnecter) du *décodage* des
// trames (`mems_session`) et du *transport* physique (`kline_transport`).
//
// Deux implémentations la fournissent :
//   - `mems_reader`   : dialogue MEMS commun (handshake + échange à écho) ;
//   - `mems19_reader` : décorateur MEMS 1.9 (réveil 5 bauds puis délégation).
typedef struct {
    // Établit la liaison (handshake). Renvoie true si l'ECU répond.
    bool (*connect)(void *ctx);
    // Émet `command` et lit exactement `expected` octets de réponse dans
    // `resp` (écho de commande inclus en tête). Renvoie true si tout est reçu
    // et l'écho correct.
    bool (*send_and_receive)(void *ctx, uint8_t command, uint8_t *resp,
                             size_t expected);
    // Marque la liaison fermée (n'agit pas sur le transport lui-même).
    void (*disconnect)(void *ctx);
    void *ctx;
} ecu_reader_t;

static inline bool ecu_reader_connect(const ecu_reader_t *r) {
    return r != NULL && r->connect != NULL && r->connect(r->ctx);
}

static inline bool ecu_reader_send_and_receive(const ecu_reader_t *r,
                                               uint8_t command, uint8_t *resp,
                                               size_t expected) {
    return r != NULL && r->send_and_receive != NULL && resp != NULL &&
           r->send_and_receive(r->ctx, command, resp, expected);
}

static inline void ecu_reader_disconnect(const ecu_reader_t *r) {
    if (r != NULL && r->disconnect != NULL) r->disconnect(r->ctx);
}
