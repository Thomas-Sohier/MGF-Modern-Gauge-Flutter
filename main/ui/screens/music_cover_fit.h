#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Géométrie de la pochette de l'écran musique et calcul du cadrage « cover »
// (façon CSS object-fit: cover) : mise à l'échelle UNIFORME de la source pour
// couvrir entièrement la fenêtre, dépassement rogné symétriquement autour du
// centre, jamais d'étirement. Pur C, header-only (aucune dépendance LVGL) :
// testé sur hôte par test/test_music_cover_fit.c.

// ── Repère 320 (cf. ui_layout.h) ────────────────────────────────────────────
// Anneau de progression : rayon de la ligne médiane et épaisseur.
#define MUSIC_RING_R 155.0f
#define MUSIC_RING_W 7.0f
// Disque de découpe de la pochette : tangent au bord intérieur de l'anneau,
// élargi d'un pixel de référence pour que l'anneau (dessiné au-dessus)
// recouvre la couture antialiasée. La pochette occupe ainsi toute la largeur
// disponible à l'intérieur de la progression.
#define MUSIC_COVER_CLIP_R  (MUSIC_RING_R - MUSIC_RING_W * 0.5f + 1.0f)
#define MUSIC_COVER_CLIP_D  (MUSIC_COVER_CLIP_R * 2.0f)
#define MUSIC_COVER_CLIP_XY (160.0f - MUSIC_COVER_CLIP_R)
// Bas de la fenêtre image : 8 px de référence au-dessus de la hauteur de
// capitale du titre (police valeur 30 px à 480, capitale ~7 px au-dessus de
// MUSIC_TRACK_Y).
#define MUSIC_COVER_IMAGE_Y1 223.0f

// Textes sous la pochette. Descendus au plus près de l'anneau tout en
// gardant chaque ligne dans la corde du disque intérieur.
#define MUSIC_TRACK_Y      238.0f
#define MUSIC_ARTIST_Y     262.0f
#define MUSIC_RULE_Y       276.0f
#define MUSIC_TIME_Y       292.0f
#define MUSIC_TRACK_WIDTH  248.0f
#define MUSIC_ARTIST_WIDTH 210.0f

// Échelle neutre des transformations (identique à LV_SCALE_NONE).
#define MUSIC_COVER_FIT_SCALE_ONE 256U
// Recouvrement ajouté de chaque côté de la fenêtre : le centrage entier de
// LVGL (demi-pixel) ne peut alors jamais laisser de liseré non couvert.
#define MUSIC_COVER_FIT_BLEED_PX 1

typedef struct {
    uint32_t scale; // uniforme, en 1/256 (256 = 1:1)
    int32_t width;  // taille de la source mise à l'échelle, >= fenêtre
    int32_t height;
} music_cover_fit_t;

static inline uint32_t music_cover_fit_ceil_div(int64_t num, int64_t den) {
    return (uint32_t)((num + den - 1) / den);
}

// Calcule l'échelle minimale qui couvre `box_w` x `box_h` (plus le
// recouvrement) avec une source `src_w` x `src_h`. Le même facteur est appliqué
// aux deux axes : les proportions de la source sont conservées. false si une
// dimension est invalide.
static inline bool music_cover_fit(int32_t src_w, int32_t src_h, int32_t box_w,
                                   int32_t box_h, music_cover_fit_t *out) {
    if (out == NULL || src_w <= 0 || src_h <= 0 || box_w <= 0 || box_h <= 0) {
        return false;
    }
    const int64_t need_w = (int64_t)box_w + 2 * MUSIC_COVER_FIT_BLEED_PX;
    const int64_t need_h = (int64_t)box_h + 2 * MUSIC_COVER_FIT_BLEED_PX;
    const uint32_t sx =
        music_cover_fit_ceil_div(need_w * MUSIC_COVER_FIT_SCALE_ONE, src_w);
    const uint32_t sy =
        music_cover_fit_ceil_div(need_h * MUSIC_COVER_FIT_SCALE_ONE, src_h);
    const uint32_t scale = sx > sy ? sx : sy;
    out->scale = scale;
    out->width = (int32_t)((int64_t)src_w * scale / MUSIC_COVER_FIT_SCALE_ONE);
    out->height = (int32_t)((int64_t)src_h * scale / MUSIC_COVER_FIT_SCALE_ONE);
    return true;
}
