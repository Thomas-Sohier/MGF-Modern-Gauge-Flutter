#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Mise en forme de la pochette : la source est une image RGB565 quelconque,
// la sortie un rectangle `width` x `height` en palette monochrome ambre
// partagée avec le thème (amber_palette.h), tramée par matrice ordonnée.
// Le cadrage est
// un « cover » façon CSS : le ratio de la source est conservé, la zone cible
// est entièrement remplie, et le dépassement est rogné symétriquement
// (horizontalement ou verticalement selon la source).
//
// Pur C (aucune dépendance LVGL/ESP) : testable sur hôte.

// Dimensions du rectangle décodé, en pixels. Indépendant de la résolution
// d'affichage : l'objet LVGL met ensuite l'image à l'échelle de la zone.
#define MUSIC_COVER_WIDTH  440
#define MUSIC_COVER_HEIGHT 308

// Convertit un pixel RGB (canaux 8 bits) en niveau ambre tramé. `dither` est
// la valeur de la matrice de Bayer 4x4 (0..15) pour la position de sortie.
// Exposé pour les tests unitaires.
uint16_t music_cover_amber_pixel(uint8_t r, uint8_t g, uint8_t b, int dither);

// Couleur RGB565 du niveau `level` (0..MUSIC_COVER_LEVELS-1, borné) : dégradé
// du fond vers l'ambre vif de la palette normale ou inversée. Le niveau L de
// la palette inversée est exactement le niveau (MUSIC_COVER_LEVELS-1-L) de la
// palette normale : les deux palettes partagent les mêmes 16 couleurs.
#define MUSIC_COVER_LEVELS 16
uint16_t music_cover_level_color(int level, bool inverted);

// Convertit en place une pochette déjà rendue (`count` pixels RGB565) de la
// palette `from_inverted` vers `to_inverted`. Chaque couleur de niveau L
// devient la couleur du niveau L dans la palette cible (permutation exacte
// L -> 15-L quand les palettes diffèrent) ; tout autre pixel est laissé
// intact. Sans perte : deux conversions successives rendent l'image
// d'origine, bit à bit. Retourne le nombre de pixels reconnus.
size_t music_cover_convert_palette(uint16_t *pixels, size_t count,
                                   bool from_inverted, bool to_inverted);

// Remplit `out` (width x height, RGB565) depuis `src` (RGB565,
// src_width x src_height) avec un crop « cover » centré. false si une
// dimension est invalide ou si la cible dépasse MUSIC_COVER_WIDTH/HEIGHT.
//
// La sortie est toujours dans la palette NORMALE (forme canonique produite
// par le worker de décodage) ; convertir ensuite avec
// music_cover_convert_palette si l'interface est en couleurs inversées.
bool music_cover_render(const uint16_t *src, int src_width, int src_height,
                        uint16_t *out, int width, int height);
