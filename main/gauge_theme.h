#pragma once

// Palette et géométrie reprises à l'identique du thème sombre Flutter MGF
// (lib/ui/themes/app_theme.dart + lib/ui/widgets/dual_arc_dial.dart).

#include "lvgl.h"

// ── Couleurs (thème sombre) ──────────────────────────────────────────────────
#define MGF_COL_BG               lv_color_hex(0x1C1C1E) // fond global (darkBg)
#define MGF_COL_GAUGE_BG         lv_color_hex(0x242426) // disque de fond de jauge
#define MGF_COL_ACTIVE           lv_color_hex(0xFFFFFF) // segments actifs (blanc)
#define MGF_COL_INACTIVE         lv_color_hex(0x3A3A3C) // segments éteints
#define MGF_COL_DANGER           lv_color_hex(0xFF453A) // zone de danger active
#define MGF_COL_DANGER_INACTIVE  lv_color_hex(0xFF453A) // (dessiné à faible opacité)
#define MGF_OPA_DANGER_INACTIVE  ((lv_opa_t)0x33)       // 0x33FF453A -> alpha 0x33
#define MGF_COL_ON_SURFACE       lv_color_hex(0xF5F5F5) // texte principal
#define MGF_COL_ON_SURFACE_DIM   lv_color_hex(0x8E8E93) // texte secondaire
#define MGF_COL_BORDER           lv_color_hex(0x48484A)

// ── Géométrie de l'arc (identique au CustomPainter Flutter) ──────────────────
#define MGF_PRIMARY_SEGMENTS      20
#define MGF_PRIMARY_SEG_HEIGHT    40.0f // épaisseur de trait de l'arc primaire
#define MGF_PRIMARY_SEG_SPACING   3.0f  // écart entre segments, en degrés
#define MGF_THROTTLE_SEG_HEIGHT   12.0f // épaisseur de l'arc "papillon" intérieur
#define MGF_THROTTLE_RADIUS_FACT  0.7f

// Demi-cercle supérieur : de 9 h (180°) à 3 h (360°), sens horaire.
// Convention LVGL identique à Flutter : 0° = 3 h, angles croissants horaires.
#define MGF_START_ANGLE_DEG       180.0f
#define MGF_SWEEP_DEG             180.0f
