#include "ui/screens/temps_screen.h"

#include "ui/widgets/amber_kit.h"

#include <math.h>
#include <stdio.h>

// L'eau est la mesure héros ; les quatre autres sondes occupent la grille
// commune.
#define COOLANT_DANGER  105.0f
#define OIL_DANGER      130.0f

enum { CELL_OIL = 0, CELL_INTAKE, CELL_AMBIENT, CELL_FUEL };

struct temps_screen_s {
    amber_page_t page;
    app_settings_units_t units;
    ecu_data_t latest;
    bool has_latest;
};

static float to_display(float celsius, app_settings_units_t units) {
    return units == APP_SETTINGS_UNITS_IMPERIAL
               ? celsius * 9.0f / 5.0f + 32.0f : celsius;
}

static const char *unit_text(app_settings_units_t units) {
    return units == APP_SETTINGS_UNITS_IMPERIAL ? "°F" : "°C";
}

temps_screen_t *temps_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    temps_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    const amber_page_spec_t spec = {
        .title = "TEMPERATURES",
        .hero_unit = unit_text(scr->units),
        .hero_caption = "EAU MOTEUR",
        .cell_caption = {"HUILE", "ADMISSION", "AMBIANTE", "CARBURANT"},
        .cell_unit = {"°C", "°C", "°C", "°C"},
        .rim = AMBER_RIM_HAIRLINE,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) {
        temps_screen_destroy(scr);
        return NULL;
    }
    amber_page_set_status(&scr->page, "PAS DE LIAISON", false);
    return scr;
}

void temps_screen_set_units(temps_screen_t *scr, app_settings_units_t units) {
    if (scr == NULL || units >= APP_SETTINGS_UNITS_COUNT || scr->units == units) {
        return;
    }
    scr->units = units;
    amber_readout_set_unit(&scr->page.hero, unit_text(units));
    for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
        amber_readout_set_unit(&scr->page.cell[i], unit_text(units));
    }
    if (scr->has_latest) temps_screen_update(scr, &scr->latest);
}

void temps_screen_update(temps_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL) return;
    scr->latest = *data;
    scr->has_latest = true;

    const bool connected = data->connected;
    const float coolant = data->coolant_temp;
    const bool coolant_ok = connected && isfinite(coolant);
    char text[16];

    amber_kit_format(text, sizeof(text), coolant_ok, "%.0f",
                     to_display(coolant, scr->units));
    amber_page_set_hero(&scr->page, text, coolant_ok);

    const float cells[AMBER_PAGE_CELLS] = {
        data->oil_temp, data->intake_air_temp, data->ambient_temp,
        data->fuel_rail_temp,
    };
    for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
        const bool ok = connected && isfinite(cells[i]);
        amber_kit_format(text, sizeof(text), ok, "%.0f",
                         to_display(cells[i], scr->units));
        amber_page_set_cell(&scr->page, i, text, ok);
    }

    const bool hot = (coolant_ok && coolant >= COOLANT_DANGER) ||
                     (connected && isfinite(data->oil_temp) &&
                      data->oil_temp >= OIL_DANGER);
    const bool cold = coolant_ok && coolant < 70.0f;
    if (!connected) {
        amber_page_set_hero_caption(&scr->page, "EAU MOTEUR", false);
        amber_page_set_status(&scr->page, "PAS DE LIAISON", false);
    } else if (hot) {
        amber_page_set_hero_caption(&scr->page, "EAU MOTEUR", true);
        amber_page_set_status(&scr->page, "ALERTE THERMIQUE", true);
    } else {
        amber_page_set_hero_caption(&scr->page, "EAU MOTEUR", false);
        amber_page_set_status(&scr->page,
                              cold ? "MOTEUR FROID" : "TEMPERATURES OK",
                              false);
    }
}

void temps_screen_destroy(temps_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    lv_free(scr);
}
