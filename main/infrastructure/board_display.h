#pragma once

// Bring-up écran + tactile de la carte Waveshare ESP32-S3-Touch-LCD-2.1
// (IPS rond 480×480, driver ST7701 en interface RGB, tactile capacitif CST820
// sur I²C, expander TCA9554 pour les resets/CS). Remplace l'ancien BSP P4.
//
// L'API reprend volontairement la forme du BSP Espressif (bsp_display_*) pour
// limiter les changements applicatifs.

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

// Initialise I²C, l'expander, le panneau ST7701 (RGB), le tactile CST820 et le
// port LVGL (thread LVGL dédié). Renvoie l'affichage LVGL prêt à l'emploi.
lv_display_t *board_display_start(void);

// Rétroéclairage (GPIO6).
void board_display_backlight_on(void);
void board_display_backlight_off(void);

// Verrou du thread LVGL pour les contextes externes : toute manipulation
// d'objets LVGL doit se faire entre lock() et unlock(). Depuis un callback LVGL
// (timer, événement…), le thread détient déjà le verrou : ne pas le reprendre.
// `timeout_ms` = 0 -> attente infinie.
bool board_display_lock(uint32_t timeout_ms);
void board_display_unlock(void);
