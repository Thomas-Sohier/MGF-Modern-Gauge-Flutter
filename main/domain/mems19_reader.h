#pragma once

#include "domain/ecu_reader.h"
#include "domain/kline_transport.h"

// Décorateur MEMS 1.9 — calqué sur `MEMS19Reader` (ecureader_mems19.go).
//
// MEMS 1.9 partage le protocole de MEMS 1.6 ; il exige seulement un réveil
// « slow init » 5 bauds (adresse ECU 0x16) avant le handshake. Ce lecteur
// enveloppe un lecteur MEMS commun : son `connect` déclenche le réveil via le
// transport puis délègue au lecteur de base ; `send_and_receive` et
// `disconnect` sont de simples délégations.
typedef struct {
    ecu_reader_t base;            // lecteur MEMS commun (délégation)
    kline_transport_t transport;  // pour le réveil 5 bauds (wake_up)
} mems19_reader_t;

// Initialise le décorateur. `base` est l'interface d'un `mems_reader` ; le
// `transport` doit fournir `wake_up` (sinon `connect` échouera).
void mems19_reader_init(mems19_reader_t *r, ecu_reader_t base,
                        kline_transport_t transport);

// Vue `ecu_reader_t` de ce décorateur (à passer à `mems_session`).
ecu_reader_t mems19_reader_interface(mems19_reader_t *r);
