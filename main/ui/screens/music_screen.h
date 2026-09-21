#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"

// Lecteur musique ambre, dessiné dans le même repère 320 px que le cadran.
// Le handle reste opaque afin que la composition de l'écran ne dépende pas de
// la hiérarchie interne des objets LVGL.
typedef struct music_screen_s music_screen_t;

// Crée une racine carrée centrée dans parent. À appeler dans le thread LVGL.
music_screen_t *music_screen_create(lv_obj_t *parent);

// Rafraîchit l'écran dans le thread LVGL avec l'état démonstrateur courant.
void music_screen_update(music_screen_t *scr, const ecu_data_t *d);

// Détruit l'écran et libère son état, avant la destruction de parent.
void music_screen_destroy(music_screen_t *scr);
