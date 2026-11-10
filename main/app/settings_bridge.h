#pragma once

// Pont réglages <-> écrans, dans la tâche LVGL : applique les réglages au
// navigateur/horloge/rétroéclairage/palette, relie l'écran de réglages (surimpression
// ouverte par glissement vers le haut ou maintien) au settings_runtime, porte le timer de réglages
// (persistance, mises à jour BLE) et la fenêtre d'appairage Bluetooth.
//
// Instance unique (stockage statique, aucune allocation). Cycle de vie :
//   settings_bridge_create  -> contexte de settings_bridge_apply ;
//   settings_bridge_start   -> écran de réglages + ouverture + timer ;
//   settings_bridge_stop    -> supprime le timer ;
//   settings_bridge_destroy -> détruit l'écran de réglages.
// stop/destroy sont sûrs même si create/start n'ont pas (entièrement) réussi.

#include <stdbool.h>

#include "app/settings_runtime.h"
#include "domain/app_settings.h"
#include "lvgl.h"
#include "ui/navigation/dashboard_navigator.h"
#include "ui/screens/clock_screen.h"
#include "ui/screens/music_screen.h"
#include "ui/screens/settings_screen.h"

#ifndef MGF_ENABLE_BLE_CONFIG
#define MGF_ENABLE_BLE_CONFIG 0
#endif

#if MGF_ENABLE_BLE_CONFIG
#include "esp_err.h"
#include "infrastructure/rtc_worker.h"
#endif

typedef struct settings_bridge_s settings_bridge_t;

typedef struct {
    lv_obj_t *screen; // écran actif : navigateur + surimpression réglages
    dashboard_navigator_t *navigator;
    clock_screen_t *clock;
    music_screen_t *music;
} settings_bridge_config_t;

settings_bridge_t *
settings_bridge_create(const settings_bridge_config_t *config);

// settings_runtime_apply_cb_t ; `context` = settings_bridge_t*.
bool settings_bridge_apply(void *context, const app_settings_t *settings);

// Crée l'écran de réglages sur `parent` (après le navigateur pour le couvrir),
// branche l'ouverture (glissement haut / maintien) du navigateur et le timer de réglages (250 ms).
bool settings_bridge_start(settings_bridge_t *bridge, lv_obj_t *parent,
                           settings_runtime_t *runtime);

settings_screen_t *settings_bridge_screen(const settings_bridge_t *bridge);

void settings_bridge_stop(settings_bridge_t *bridge);
void settings_bridge_destroy(settings_bridge_t *bridge);

#if MGF_ENABLE_BLE_CONFIG
// Démarre le service BLE de réglages (lecture/écriture des réglages, mise à
// l'heure UTC via le worker RTC facultatif).
esp_err_t settings_bridge_start_ble(settings_runtime_t *runtime,
                                    rtc_worker_t *rtc_worker,
                                    bool pairing_open_on_start);
#endif
