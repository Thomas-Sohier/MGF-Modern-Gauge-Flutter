#pragma once

#include <stdbool.h>

#include "lvgl.h"
#include "domain/companion_protocol.h"

// Écran de guidage ambre, vectoriel et adapté au panneau rond 480 x 480,
// alimenté par l'application compagnon (navigation du téléphone).
typedef struct navigation_screen_s navigation_screen_t;

// `parent` doit rester vivant jusqu'à navigation_screen_destroy(). Les appels
// doivent être effectués dans le thread LVGL (ou sous son verrou).
navigation_screen_t *navigation_screen_create(lv_obj_t *parent);
// Dernier état de guidage (NULL ou inactif : aucun guidage).
void navigation_screen_set_route(navigation_screen_t *scr,
                                 const companion_nav_t *nav);
// Téléphone appairé connecté ; la perte du lien efface le guidage.
void navigation_screen_set_link(navigation_screen_t *scr, bool linked);
void navigation_screen_destroy(navigation_screen_t *scr);
