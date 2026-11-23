#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "domain/ecu_reader.h"
#include "domain/kline_transport.h"
#include "domain/mems_protocol.h"

// Pause appliquée entre deux commandes consécutives sur la K-line half-duplex.
// ISO-9141 (timing P3) impose un délai minimal à l'ECU pour relâcher la ligne
// et se remettre en écoute : sans elle, une commande émise juste après une
// réponse peut être perdue (l'ECU est encore en train de transmettre).
#define MEMS_INTER_COMMAND_DELAY_MS 5u

// Implémentation COMMUNE du lecteur MEMS (partagée par 1.6 et 1.9).
//
// Porte le dialogue de `ecureader_mems.go` : handshake d'init CA/75/F4/D0 et
// échange commande/réponse à écho, au-dessus d'un `kline_transport_t`. Ne
// connaît ni la variante d'ECU ni le décodage des trames — ceux-ci sont
// respectivement dans `mems19_reader` et `mems_session`.
typedef struct {
    kline_transport_t transport;
    uint32_t read_timeout_ms; // délai max par lecture d'octet(s)
    bool connected;           // vrai après un handshake réussi
    // Vrai dès qu'au moins une commande a été émise avec succès : la pause
    // inter-commande n'a de sens qu'à partir du 2e échange (l'ECU doit d'abord
    // avoir vu une trame pour être en train de répondre).
    bool has_exchanged;
    uint8_t ecu_id[MEMS_RESP_ECU_ID]; // réponse D0 (écho + ID) du dernier init
} mems_reader_t;

// Initialise le lecteur commun avec un transport et un délai de lecture.
void mems_reader_init(mems_reader_t *r, kline_transport_t transport,
                      uint32_t read_timeout_ms);

// Vue `ecu_reader_t` de ce lecteur (à passer à `mems_session` ou à envelopper
// dans un `mems19_reader`).
ecu_reader_t mems_reader_interface(mems_reader_t *r);

// Réponse D0 (écho + 4 octets d'ID ECU) du dernier handshake réussi.
static inline const uint8_t *mems_reader_ecu_id(const mems_reader_t *r) {
    return r != NULL ? r->ecu_id : NULL;
}
