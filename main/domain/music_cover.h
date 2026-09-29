#pragma once

#include <stdbool.h>
#include <stdint.h>

// Mise en forme de la pochette : la source est une image RGB565 quelconque,
// la sortie un rectangle `width` x `height` en palette monochrome ambre
// (fond #1B1712 -> ambre #FFB51B) tramée par matrice ordonnée. Le cadrage est
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

// Remplit `out` (width x height, RGB565) depuis `src` (RGB565,
// src_width x src_height) avec un crop « cover » centré. false si une
// dimension est invalide ou si la cible dépasse MUSIC_COVER_WIDTH/HEIGHT.
bool music_cover_render(const uint16_t *src, int src_width, int src_height,
                        uint16_t *out, int width, int height);
