#include "ui/screens/temps_screen.h"

#include "domain/temperature_status.h"
#include "ui/widgets/amber_kit.h"

#include <math.h>
#include <stdio.h>

// L'eau est la mesure héros ; les quatre autres sondes occupent la grille
// commune.

enum { CELL_OIL = 0, CELL_INTAKE, CELL_AMBIENT, CELL_FUEL };

struct temps_screen_s {
    amber_page_t page;
    app_settings_units_t units;
    ecu_data_t latest;
    bool has_latest;
};

static float to_display(float celsius, app_settings_units_t units) {
    return units == APP_SETTINGS_UNITS_IMPERIAL ? celsius * 9.0f / 5.0f + 32.0f
                                                : celsius;
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
        .cell_caption = {"HUILE", "ADMIS.", "AMBIANTE", "CARBU."},
        .cell_unit = {"°C", "°C", "°C", "°C"},
        .rim = AMBER_RIM_HAIRLINE,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) {
        temps_screen_destroy(scr);
        return NULL;
    }
    amber_page_set_status(&scr->page, "ECU NON CONNECTEE", false);
    return scr;
}

void temps_screen_set_units(temps_screen_t *scr, app_settings_units_t units) {
    if (scr == NULL || units >= APP_SETTINGS_UNITS_COUNT ||
        scr->units == units) {
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
        data->oil_temp,
        data->intake_air_temp,
        data->ambient_temp,
        data->fuel_rail_temp,
    };
    for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
        const bool ok = connected && isfinite(cells[i]);
        amber_kit_format(text, sizeof(text), ok, "%.0f",
                         to_display(cells[i], scr->units));
        amber_page_set_cell(&scr->page, i, text, ok);
    }

    const temperature_status_t status = temperature_status_of(data);
    const char *status_text;
    bool status_alert = false;
    switch (status) {
    case TEMPERATURE_STATUS_NO_LINK:
        status_text = "ECU NON CONNECTEE";
        break;
    case TEMPERATURE_STATUS_THERMAL_ALERT:
        status_text = "ALERTE THERMIQUE";
        status_alert = true;
        break;
    case TEMPERATURE_STATUS_COOLANT_SENSOR_FAULT:
        status_text = "DEFAUT SONDE EAU";
        break;
    case TEMPERATURE_STATUS_COOLANT_UNAVAILABLE:
        status_text = "EAU INDISPONIBLE";
        break;
    case TEMPERATURE_STATUS_COLD:
        status_text = "MOTEUR FROID";
        break;
    case TEMPERATURE_STATUS_OK:
    default:
        status_text = "TEMPERATURES OK";
        break;
    }
    amber_page_set_hero_caption(&scr->page, "EAU MOTEUR", status_alert);
    amber_page_set_status(&scr->page, status_text, status_alert);
}

void temps_screen_destroy(temps_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    lv_free(scr);
}
