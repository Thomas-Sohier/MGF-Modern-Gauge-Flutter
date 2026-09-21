#pragma once

#include <stdbool.h>

#include "domain/ecu_data.h"
#include "domain/ecu_reader.h"
#include "domain/mems_protocol.h"

// Couche de poll + décodage MEMS — calquée sur `ECUReaderInstance.GetDataframes`
// du projet Go. Agnostique de la variante et du transport : elle pilote un
// `ecu_reader_t` (MEMS 1.6 ou 1.9, indifféremment) et décode les trames en
// `ecu_data_t`.
typedef struct {
    ecu_reader_t reader;
    bool connected;  // reflète le dernier connect/poll
} mems_session_t;

// Initialise la session avec un lecteur ECU déjà construit.
void mems_session_init(mems_session_t *s, ecu_reader_t reader);

// Établit la liaison via le lecteur (handshake, et réveil 5 bauds si 1.9).
// Positionne `connected`.
bool mems_session_connect(mems_session_t *s);

// Interroge les trames 0x80 puis 0x7D, décode et remplit `out` (connected=true).
// Renvoie false en cas d'erreur de lecture / trame invalide ; dans ce cas
// `connected` repasse à false et `out` n'est pas modifié. `raw` non-NULL reçoit
// l'instantané MEMS détaillé.
bool mems_session_poll(mems_session_t *s, ecu_data_t *out, mems_data_t *raw);

// Marque la session déconnectée et prévient le lecteur.
void mems_session_disconnect(mems_session_t *s);
