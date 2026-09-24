#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "domain/app_settings.h"
#include "domain/ecu_data.h"
#include "lvgl.h"

typedef void (*dashboard_page_update_cb_t)(void *context,
                                            const ecu_data_t *data);
typedef void (*dashboard_page_destroy_cb_t)(void *context);
typedef void (*dashboard_page_settings_cb_t)(
    void *context, app_settings_units_t units);
typedef void (*dashboard_page_changed_cb_t)(void *context, size_t page_index);
typedef void (*dashboard_hold_cb_t)(void *context);

typedef struct {
    const char *name;
    void *context;
    dashboard_page_update_cb_t update;
    dashboard_page_destroy_cb_t destroy;
    dashboard_page_settings_cb_t settings_changed;
    // 0 désactive les mises à jour ECU après l'activation : la page est
    // événementielle (par exemple musique ou navigation statique).
    uint32_t update_period_ms;
} dashboard_page_t;

typedef struct dashboard_navigator_s dashboard_navigator_t;

// Crée une pile de pages plein écran. Un glissement horizontal ou un appui sur
// le tiers gauche/droit sélectionne cycliquement la page précédente/suivante.
dashboard_navigator_t *dashboard_navigator_create(lv_obj_t *parent);

// Crée et retourne le conteneur d'une nouvelle page. Le contenu doit ensuite
// être construit dans ce conteneur puis associé avec register_page().
lv_obj_t *dashboard_navigator_create_page(dashboard_navigator_t *navigator);
bool dashboard_navigator_register_page(dashboard_navigator_t *navigator,
                                       lv_obj_t *page,
                                       const dashboard_page_t *descriptor);

void dashboard_navigator_next(dashboard_navigator_t *navigator);
void dashboard_navigator_previous(dashboard_navigator_t *navigator);
// Sélectionne une page déjà enregistrée. Retourne false pour un index hors
// limites. Une sélection identique ne déclenche pas l'observateur.
bool dashboard_navigator_select_page(dashboard_navigator_t *navigator,
                                      size_t page_index);
// Installe un observateur appelé après chaque changement réel de page.
void dashboard_navigator_set_page_changed_callback(
    dashboard_navigator_t *navigator, dashboard_page_changed_cb_t callback,
    void *context);
// Installe l'action du maintien prolongé (~1 s, doigt immobile, dans le
// disque visible) : ouverture des réglages. Le relâcher n'est pas un tap.
void dashboard_navigator_set_hold_callback(dashboard_navigator_t *navigator,
                                            dashboard_hold_cb_t callback,
                                            void *context);
// Propagates the unit preference to pages that display physical quantities.
// Must run in the LVGL task (or under its platform lock).
void dashboard_navigator_set_units(dashboard_navigator_t *navigator,
                                   app_settings_units_t units);
size_t dashboard_navigator_current(const dashboard_navigator_t *navigator);
size_t dashboard_navigator_count(const dashboard_navigator_t *navigator);
const char *dashboard_navigator_current_name(
    const dashboard_navigator_t *navigator);

// Mémorise le dernier instantané ECU et actualise uniquement la page visible.
// Lors d'une navigation, la nouvelle page reçoit immédiatement cet instantané.
void dashboard_navigator_update(dashboard_navigator_t *navigator,
                                const ecu_data_t *data);
void dashboard_navigator_destroy(dashboard_navigator_t *navigator);
