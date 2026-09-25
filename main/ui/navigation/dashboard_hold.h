#pragma once

#include <stdbool.h>
#include <stdint.h>

// Décision pure du maintien prolongé (~1 s) de la navigation, sans LVGL :
// testable sur hôte. Le même état sert aussi à distinguer un tap d'un
// glissement lent qui ne doit ni ouvrir les réglages ni déclencher de
// navigation au relâchement.
#define DASHBOARD_HOLD_MS 1000U

// Déplacement maximal toléré : 18 px à la résolution de référence 480 px.
// Le seuil est mis à l'échelle par min(width, height) / 480 pour rester
// proportionnel à l'écran.
#define DASHBOARD_HOLD_MOVE_PX  18
#define DASHBOARD_HOLD_MOVE_REF 480

typedef struct {
    bool tracking;  // une pression est mémorisée
    bool cancelled; // déplacement trop grand ou geste LVGL : plus de maintien
    bool fired;     // maintien déjà déclenché une fois pour cette pression
    uint32_t start_tick;
    int32_t start_x;
    int32_t start_y;
} dashboard_hold_t;

// Repart d'un état neutre (aucune pression en cours).
static inline void dashboard_hold_reset(dashboard_hold_t *hold) {
    *hold = (dashboard_hold_t){0};
}

// Mémorise la position absolue et l'instant du PRESSED. Réarme le maintien :
// une nouvelle pression repart d'un état propre.
static inline void dashboard_hold_begin(dashboard_hold_t *hold, uint32_t tick,
                                        int32_t x, int32_t y) {
    hold->tracking = true;
    hold->cancelled = false;
    hold->fired = false;
    hold->start_tick = tick;
    hold->start_x = x;
    hold->start_y = y;
}

// Annule le maintien pour toute la pression (geste LVGL). Ne se réarme qu'au
// prochain PRESSED.
static inline void dashboard_hold_cancel(dashboard_hold_t *hold) {
    hold->cancelled = true;
}

// Compare le déplacement euclidien au point initial. Au-delà du seuil, le
// maintien est annulé définitivement, même si le doigt revient au départ.
// Retourne true si le maintien est annulé. Les calculs restent en int64 :
// un axe plus long que le seuil court-circuite le carré pour éviter tout
// débordement.
static inline bool dashboard_hold_move_cancelled(dashboard_hold_t *hold,
                                                 int32_t x, int32_t y,
                                                 int32_t min_dimension) {
    if (!hold->tracking || hold->cancelled) return hold->cancelled;
    if (min_dimension <= 0) min_dimension = 1;

    const int64_t dx = (int64_t)x - hold->start_x;
    const int64_t dy = (int64_t)y - hold->start_y;
    const int64_t ax = dx < 0 ? -dx : dx;
    const int64_t ay = dy < 0 ? -dy : dy;
    const int64_t limit = ((int64_t)DASHBOARD_HOLD_MOVE_PX * min_dimension) /
                          DASHBOARD_HOLD_MOVE_REF;

    // 18 px pile à 480 px est autorisé ; 19 px annule. En diagonale, la
    // distance euclidienne (13, 13) ≈ 18,4 px annule donc aussi.
    if (ax > limit || ay > limit || ax * ax + ay * ay > limit * limit) {
        hold->cancelled = true;
    }
    return hold->cancelled;
}

// Décide le déclenchement : après >= 1000 ms, doigt dans le disque, jamais
// deux fois pour la même pression. Le wrap du tick uint32 est géré par la
// soustraction modulaire. Retourne true une seule fois par pression.
static inline bool dashboard_hold_should_fire(dashboard_hold_t *hold,
                                              uint32_t now, bool in_disc) {
    if (!hold->tracking || hold->cancelled || hold->fired) return false;
    if (!in_disc) return false;
    if ((uint32_t)(now - hold->start_tick) < DASHBOARD_HOLD_MS) return false;
    hold->fired = true;
    return true;
}
