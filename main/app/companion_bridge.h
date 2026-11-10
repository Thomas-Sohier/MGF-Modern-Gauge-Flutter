#pragma once

// Pont application compagnon -> écrans, dans la tâche LVGL. Les messages sont
// décodés par la tâche NimBLE (companion_gatt.c) ; le timer du pont (200 ms)
// ne fait que les consommer et les appliquer : musique, pochette (via le
// worker de décodage hors LVGL), navigation, heure + fuseau, télécommande.
// Sans BLE (MGF_ENABLE_BLE_CONFIG=0), le timer ne fait qu'animer la musique.
//
// Instance unique (stockage statique). companion_bridge_destroy est sûr même
// si companion_bridge_start n'a pas été appelé ou a échoué.

#include <stdbool.h>

#include "app/settings_runtime.h"
#include "infrastructure/rtc_worker.h"
#include "ui/navigation/dashboard_navigator.h"
#include "ui/screens/music_screen.h"
#include "ui/screens/navigation_screen.h"
#include "ui/screens/settings_screen.h"

typedef struct {
    dashboard_navigator_t *navigator;
    music_screen_t *music;
    navigation_screen_t *navigation;
    settings_runtime_t *settings_runtime; // fuseau reçu du téléphone
    settings_screen_t *settings_screen;   // fermé par la télécommande
    rtc_worker_t *rtc_worker;             // facultatif (NULL sans DS3231)
} companion_bridge_config_t;

// Démarre le worker de pochettes (BLE) puis crée le timer du pont.
bool companion_bridge_start(const companion_bridge_config_t *config);

// Supprime le timer puis arrête le worker de pochettes (BLE).
void companion_bridge_destroy(void);
