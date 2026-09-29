#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "domain/companion_art.h"
#include "domain/music_cover.h"

// Worker de décodage de la pochette, hors tâche LVGL.
//
// Le réassemblage GATT dépose un JPEG complet (propriété transférée). Une
// tâche FreeRTOS dédiée le décode en ambre monochrome (tjpgd + music_cover)
// puis publie le rectangle RGB565 dans une boîte aux lettres lue par le timer
// LVGL. Le rendu LVGL n'est jamais appelé ici ; l'allocation du JPEG et du
// résultat se fait en PSRAM.
//
// `submit` prend TOUJOURS possession du tampon, même en cas d'échec (dernier
// gagnant : une pochette en attente est libérée). `take` transfère la
// propriété du résultat à l'appelant, qui doit le libérer avec
// `companion_art_worker_free`.

void companion_art_worker_start(void);
// Dépose un JPEG complet (propriété transférée). false si le worker est arrêté
// (le tampon est libéré dans tous les cas).
bool companion_art_worker_submit(companion_art_jpeg_t *art);
// Récupère le dernier résultat décodé sans blocage (propriété transférée).
bool companion_art_worker_take(uint16_t **pixels, int *width, int *height);
// Libère un buffer rendu par `take` (PSRAM ou tas hôte).
void companion_art_worker_free(uint16_t *pixels);
// Arrête la tâche, purge les tampons en attente. Idempotent.
void companion_art_worker_stop(void);
