#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "domain/ecu_data.h"
#include "lvgl.h"

typedef void (*dashboard_page_update_cb_t)(void *context,
                                            const ecu_data_t *data);
typedef void (*dashboard_page_destroy_cb_t)(void *context);

typedef struct {
    const char *name;
    void *context;
    dashboard_page_update_cb_t update;
    dashboard_page_destroy_cb_t destroy;
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
size_t dashboard_navigator_current(const dashboard_navigator_t *navigator);
size_t dashboard_navigator_count(const dashboard_navigator_t *navigator);
const char *dashboard_navigator_current_name(
    const dashboard_navigator_t *navigator);

// Mémorise le dernier instantané ECU et actualise uniquement la page visible.
// Lors d'une navigation, la nouvelle page reçoit immédiatement cet instantané.
void dashboard_navigator_update(dashboard_navigator_t *navigator,
                                const ecu_data_t *data);
void dashboard_navigator_destroy(dashboard_navigator_t *navigator);
