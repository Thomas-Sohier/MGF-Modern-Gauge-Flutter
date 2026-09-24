#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "domain/companion_protocol.h"

// Lecteur musique ambre : morceau en cours sur le téléphone, reçu de
// l'application compagnon. Le handle reste opaque afin que la composition de
// l'écran ne dépende pas de la hiérarchie interne des objets LVGL. Toutes les
// fonctions s'appellent dans le thread LVGL.
typedef struct music_screen_s music_screen_t;

// Crée une racine carrée centrée dans parent (état : téléphone déconnecté).
music_screen_t *music_screen_create(lv_obj_t *parent);

// Dernier état reçu (NULL : aucun média). `now_ms` date la position reçue,
// extrapolée ensuite pendant la lecture.
void music_screen_set_media(music_screen_t *scr, const companion_media_t *media,
                            uint32_t now_ms);
// Téléphone appairé connecté ou non.
void music_screen_set_link(music_screen_t *scr, bool linked, uint32_t now_ms);
// Avance position et anneau ; à appeler périodiquement (page visible).
void music_screen_tick(music_screen_t *scr, uint32_t now_ms);

// Détruit l'écran et libère son état, avant la destruction de parent.
void music_screen_destroy(music_screen_t *scr);
