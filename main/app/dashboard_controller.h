#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "app/app_state.h"
#include "domain/ecu_source.h"
#include "ui/screens/style_amber.h"

typedef struct {
    amber_screen_t *screen;
    ecu_source_t ecu_source;
    uint32_t period_ms;
} dashboard_controller_config_t;

typedef struct dashboard_controller_s dashboard_controller_t;

// Construit le contrôleur. La source et l'écran sont empruntés (ils doivent
// rester valides jusqu'à dashboard_controller_destroy()). Ils sont utilisés par
// le callback d'un timer LVGL, donc dans le thread LVGL.
dashboard_controller_t *dashboard_controller_create(
    const dashboard_controller_config_t *config);

// Démarre le timer LVGL du contrôleur. À appeler depuis un contexte externe
// après acquisition du verrou LVGL (ou directement dans le thread LVGL).
bool dashboard_controller_start(dashboard_controller_t *controller);

// Retourne le dernier état observé par le contrôleur. Une lecture réussie
// avec connected=false produit DASHBOARD_STATE_DISCONNECTED.
dashboard_state_t dashboard_controller_state(
    const dashboard_controller_t *controller);

// Retourne le nombre de lectures ECU en erreur depuis la création du
// contrôleur. Une lecture réussie, y compris déconnectée, ne l'incrémente pas.
uint32_t dashboard_controller_error_count(
    const dashboard_controller_t *controller);

// Arrête le timer et libère le contrôleur. Le contrôleur ne détruit pas la
// source ni l'écran empruntés. À appeler depuis le thread LVGL, hors du
// callback du contrôleur.
void dashboard_controller_destroy(dashboard_controller_t *controller);
