#pragma once

#include <math.h>
#include <stdint.h>

// Lissage exponentiel du premier ordre pour l'affichage. Les trames MEMS
// arrivent à ~5 Hz alors que l'écran se rafraîchit à 25 Hz : sans filtre, le
// compte-tours avance par paliers. `tau_ms` est la constante de temps (63 %
// du saut atteint après tau_ms).
//
// - cible non finie (mesure indisponible) : renvoie NAN, pas d'interpolation ;
// - état courant non fini (premier échantillon, retour de liaison) : saute
//   directement à la cible ;
// - écart résiduel < `snap` : saute à la cible pour se stabiliser.
static inline float value_smoothing_step(float current, float target,
                                         uint32_t elapsed_ms, float tau_ms,
                                         float snap) {
    if (!isfinite(target)) return NAN;
    if (!isfinite(current) || tau_ms <= 0.0f) return target;
    const float alpha = 1.0f - expf(-(float)elapsed_ms / tau_ms);
    const float next = current + (target - current) * alpha;
    return fabsf(target - next) < snap ? target : next;
}
