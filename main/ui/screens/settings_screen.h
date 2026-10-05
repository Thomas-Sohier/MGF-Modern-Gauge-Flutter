#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "domain/app_settings.h"
#include "lvgl.h"

// Écran de réglages ambre, affiché en surimpression du dashboard (glissement
// vers le haut ou maintien prolongé). Il ne touche ni aux réglages ni au BLE :
// chaque action est remontée à l'application, qui applique puis resynchronise
// l'écran.
typedef struct settings_screen_s settings_screen_t;

typedef enum {
    SETTINGS_BLE_UNAVAILABLE = 0, // image sans BLE ou service non démarré
    SETTINGS_BLE_CLOSED,
    SETTINGS_BLE_OPEN,      // fenêtre d'appairage ouverte
    SETTINGS_BLE_CONNECTED, // téléphone appairé lié
} settings_ble_state_t;

typedef struct {
    void *context;
    void (*brightness_changed)(void *context, uint8_t brightness_percent);
    // Page fixe, ou APP_SETTINGS_STARTUP_LAST_PAGE.
    void (*startup_page_changed)(void *context, app_settings_page_t page);
    // true : ouvrir la fenêtre d'appairage BLE ; false : la fermer.
    void (*bluetooth_toggled)(void *context, bool open);
    // Palette normale ou inversée (appliquée instantanément par l'application).
    void (*color_mode_changed)(void *context, app_settings_color_mode_t mode);
    void (*closed)(void *context);
} settings_screen_actions_t;

// Créé masqué. `parent` est l'écran actif : la surimpression le couvre
// entièrement et intercepte tous les appuis tant qu'elle est visible.
settings_screen_t *
settings_screen_create(lv_obj_t *parent,
                       const settings_screen_actions_t *actions);
void settings_screen_show(settings_screen_t *screen,
                          const app_settings_t *settings);
void settings_screen_hide(settings_screen_t *screen);
bool settings_screen_is_visible(const settings_screen_t *screen);
// Resynchronise l'affichage après application d'un réglage.
void settings_screen_set_settings(settings_screen_t *screen,
                                  const app_settings_t *settings);
void settings_screen_set_bluetooth(settings_screen_t *screen,
                                   settings_ble_state_t state,
                                   uint32_t remaining_s);
void settings_screen_destroy(settings_screen_t *screen);
