#pragma once

#include <stdbool.h>
#include <stdint.h>

// Borne de fraîcheur d'un instantané MEMS.
//
// L'ECU est interrogée à ~5 Hz (200 ms) et un instantané est publié à chaque
// poll réussi. La session tolère MEMS_SESSION_MAX_POLL_FAILURES trames perdues
// consécutives avant de se déclarer déconnectée : pendant cette fenêtre le
// dernier instantané reste marqué `connected`. Présenter ces chiffres figés
// comme des mesures actuelles serait trompeur.
//
// 1,5 s est une borne UX volontairement conservatrice (bien au-delà de la
// période de poll), pas une mesure du temps de propagation K-line : c'est
// simplement le délai au bout duquel on préfère afficher « -- » plutôt qu'une
// valeur périmée. Le seuil n'agit qu'à la lecture, sans toucher à la cadence ni
// à la session.
#define ECU_FRESHNESS_MAX_AGE_US 1500000

// Indique si l'instantané horodaté `stamp_us` est encore frais à `now_us`.
//
// - `has_snapshot == false` : aucun instantané publié -> non frais ;
// - `now_us < stamp_us` (horloge remise à zéro, incohérence) : non frais ;
// - sinon frais tant que l'âge est strictement inférieur au seuil (un âge de
//   exactement ECU_FRESHNESS_MAX_AGE_US est donc périmé).
//
// Les lectures de l'horloge monotone sont converties en `uint64_t` (elles sont
// positives) pour que la soustraction d'âge ne puisse pas déborder en signé.
static inline bool ecu_snapshot_is_fresh(bool has_snapshot, uint64_t stamp_us,
                                         uint64_t now_us) {
    if (!has_snapshot) return false;
    if (now_us < stamp_us) return false;
    return (now_us - stamp_us) < ECU_FRESHNESS_MAX_AGE_US;
}
