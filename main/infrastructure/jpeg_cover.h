#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Décodage JPEG (tjpgd fourni par LVGL) vers la pochette ambre monochrome.
//
// Le décodage produit une image RGB565 intermédiaire (PSRAM sur cible), puis
// `music_cover_render` applique le cadrage « cover » et la palette ambre. Le
// résultat est un rectangle `width` x `height` (typiquement
// MUSIC_COVER_WIDTH x MUSIC_COVER_HEIGHT) que l'appelant fournit. Aucune
// dépendance ESP-IDF hors allocation conditionnelle.
bool jpeg_cover_decode_amber(const uint8_t *jpeg, size_t length, int width,
                             int height, uint16_t *out);
