#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "domain/ecu_data.h"
#include "domain/ecu_reader.h"
#include "domain/mems_protocol.h"

// Nombre d'échecs de poll consécutifs tolérés avant de déclarer la liaison
// perdue. Une trame isolée corrompue par le bruit K-line ne doit pas faire
// clignoter l'affichage ni relancer tout le handshake.
#define MEMS_SESSION_MAX_POLL_FAILURES 3

// Par défaut, la trame 0x7D (papillon, lambda… : grandeurs lentes) n'est
// interrogée qu'un cycle sur 3 ; la trame 0x80 (régime) l'est à chaque cycle.
#define MEMS_SESSION_DEFAULT_7D_INTERVAL 3u

// Couche de poll + décodage MEMS — calquée sur `ECUReaderInstance.GetDataframes`
// du projet Go. Agnostique de la variante et du transport : elle pilote un
// `ecu_reader_t` (MEMS 1.6 ou 1.9, indifféremment) et décode les trames en
// `ecu_data_t`. Champs internes : passer par les accesseurs ci-dessous.
typedef struct {
    ecu_reader_t reader;
    bool connected; // reflète le dernier connect/poll
    uint8_t
        poll_failures;   // échecs de poll consécutifs depuis le dernier succès
    uint8_t interval_7d; // 0x7D interrogée un cycle sur `interval_7d` (>= 1)
    uint8_t cycles_since_7d; // cycles réussis depuis la dernière 0x7D
    bool has_7d;      // `last` contient une 0x7D valide de CETTE connexion
    mems_data_t last; // dernier instantané décodé (champs 0x7D conservés)
} mems_session_t;

// Initialise la session avec un lecteur ECU déjà construit (intervalle 0x7D
// par défaut : MEMS_SESSION_DEFAULT_7D_INTERVAL).
void mems_session_init(mems_session_t *s, ecu_reader_t reader);

// Règle la fréquence de la trame 0x7D : interrogée un cycle sur `every`
// (0 est traité comme 1 = à chaque cycle).
void mems_session_set_7d_interval(mems_session_t *s, uint8_t every);

// Établit la liaison via le lecteur (handshake, et réveil 5 bauds si 1.9).
// Positionne `connected` et oublie tout instantané d'une connexion précédente :
// le premier poll suivant interroge donc toujours 0x80 ET 0x7D.
bool mems_session_connect(mems_session_t *s);

// Un cycle de poll : interroge 0x80 à chaque appel, et 0x7D si elle est due
// (un cycle sur `interval_7d`, ou si aucune 0x7D valide n'a encore été reçue
// sur cette connexion). Entre deux 0x7D, ses champs gardent leur dernière
// valeur valide. Remplit `out` (connected=true) en cas de succès.
// Renvoie false si un échange ou un décodage échoue (trame perdue) ; `out`
// et l'instantané interne ne sont alors pas modifiés, et une 0x7D due reste
// due. `connected` ne repasse à false qu'après MEMS_SESSION_MAX_POLL_FAILURES
// échecs consécutifs. `raw` non-NULL reçoit l'instantané MEMS détaillé.
bool mems_session_poll(mems_session_t *s, ecu_data_t *out, mems_data_t *raw);

// Marque la session déconnectée et prévient le lecteur.
void mems_session_disconnect(mems_session_t *s);

static inline bool mems_session_is_connected(const mems_session_t *s) {
    return s != NULL && s->connected;
}

static inline uint8_t mems_session_poll_failures(const mems_session_t *s) {
    return s != NULL ? s->poll_failures : 0;
}
