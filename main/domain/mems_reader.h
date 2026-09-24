#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "domain/ecu_reader.h"
#include "domain/kline_transport.h"
#include "domain/mems_protocol.h"

// Implémentation COMMUNE du lecteur MEMS (partagée par 1.6 et 1.9).
//
// Porte le dialogue de `ecureader_mems.go` : handshake d'init CA/75/F4/D0 et
// échange commande/réponse à écho, au-dessus d'un `kline_transport_t`. Ne
// connaît ni la variante d'ECU ni le décodage des trames — ceux-ci sont
// respectivement dans `mems19_reader` et `mems_session`.
typedef struct {
    kline_transport_t transport;
    uint32_t read_timeout_ms;         // délai max par lecture d'octet(s)
    bool connected;                   // vrai après un handshake réussi
    uint8_t ecu_id[MEMS_RESP_ECU_ID]; // réponse D0 (écho + ID) du dernier init
} mems_reader_t;

// Initialise le lecteur commun avec un transport et un délai de lecture.
void mems_reader_init(mems_reader_t *r, kline_transport_t transport,
                      uint32_t read_timeout_ms);

// Vue `ecu_reader_t` de ce lecteur (à passer à `mems_session` ou à envelopper
// dans un `mems19_reader`).
ecu_reader_t mems_reader_interface(mems_reader_t *r);
