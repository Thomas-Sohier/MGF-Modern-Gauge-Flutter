#pragma once

#include "lvgl.h"

// Jauge à double arc : arc primaire segmenté (extérieur) + arc "papillon"
// continu (intérieur), dessinés en une seule passe via un événement de dessin
// LVGL — l'équivalent embarqué du CustomPainter `DualArcDial` de Flutter.
//
// Aucune animation interne : à ~20-30 Hz de données, un tween n'apporterait
// rien de visible et forcerait des repaints par frame. La jauge n'est
// invalidée que lorsqu'une valeur change réellement.

// Crée la jauge comme enfant de `parent` (elle remplit le parent).
lv_obj_t *dual_arc_dial_create(lv_obj_t *parent);

// Met à jour les valeurs affichées. Les paramètres `max`/`danger` décrivent la
// métrique primaire courante (ici le RPM). N'invalide la jauge que si une
// valeur a changé (resolution normalisée, cf. hasGaugeValueChanged Flutter).
void dual_arc_dial_set_values(lv_obj_t *dial,
                              float throttle, float throttle_max,
                              float primary, float primary_max,
                              float primary_danger);
