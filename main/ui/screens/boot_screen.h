#pragma once

#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct boot_screen boot_screen_t;

// Mode d'affichage de l'écran de démarrage :
// - BOOT_SCREEN_MODE_2D_MINIMAL : logo MG octogonal 2D pur (conservé intact).
// - BOOT_SCREEN_MODE_3D_LOGO    : emblème MG octogonal en relief 2.5D/3D qui
//                                 tourne sur lui-même.
typedef enum {
    BOOT_SCREEN_MODE_2D_MINIMAL = 0,
    BOOT_SCREEN_MODE_3D_LOGO,
} boot_screen_mode_t;

// Durée de la séquence d'accueil (logo MG 3D en rotation) avant d'effacer la
// surimpression vers l'écran d'accueil.
#define BOOT_SCREEN_INTRO_TOTAL_MS 2000U
#define BOOT_SCREEN_TICK_PERIOD_MS 50U

boot_screen_t *boot_screen_create(lv_obj_t *parent);
void boot_screen_set_mode(boot_screen_t *screen, boot_screen_mode_t mode);
void boot_screen_set_elapsed(boot_screen_t *screen, uint32_t elapsed_ms);
// Démarre l'animation autonome de démarrage (timer LVGL à 20 Hz, porte la
// surimpression au premier plan, fait tourner le logo MG 3D, et
// détruit automatiquement le boot screen à la fin de la séquence).
void boot_screen_start_intro(boot_screen_t *screen);
void boot_screen_destroy(boot_screen_t *screen);
