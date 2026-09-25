#include "ui/screens/admission_screen.h"

#include "ui/widgets/amber_kit.h"

#include <math.h>

// Pression collecteur (MAP) en héros, organes d'admission dans la grille.

// Seuils d'ouverture papillon pour l'état de charge affiché en ligne d'état.
#define THROTTLE_IDLE_MAX_PCT 3
#define THROTTLE_FULL_MIN_PCT 90

enum { CELL_THROTTLE = 0, CELL_TPS, CELL_INTAKE, CELL_RPM };

struct admission_screen_s {
    amber_page_t page;
    app_settings_units_t units;
    ecu_data_t latest;
    bool has_latest;
};

static bool imperial(app_settings_units_t units) {
    return units == APP_SETTINGS_UNITS_IMPERIAL;
}

admission_screen_t *admission_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    admission_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    const amber_page_spec_t spec = {
        .title = "ADMISSION",
        .hero_unit = "kPa",
        .hero_caption = "PRESSION MAP",
        .cell_caption = {"PAPILLON", "TPS", "AIR ADMIS", "REGIME"},
        .cell_unit = {"%", "V", "°C", "RPM"},
        .rim = AMBER_RIM_HAIRLINE,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) {
        admission_screen_destroy(scr);
        return NULL;
    }
    amber_page_set_status(&scr->page, "ECU NON CONNECTEE", false);
    return scr;
}

void admission_screen_set_units(admission_screen_t *scr,
                                app_settings_units_t units) {
    if (scr == NULL || units >= APP_SETTINGS_UNITS_COUNT ||
        scr->units == units) {
        return;
    }
    scr->units = units;
    amber_readout_set_unit(&scr->page.hero, imperial(units) ? "psi" : "kPa");
    amber_readout_set_unit(&scr->page.cell[CELL_INTAKE],
                           imperial(units) ? "°F" : "°C");
    if (scr->has_latest) admission_screen_update(scr, &scr->latest);
}

void admission_screen_update(admission_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL) return;
    scr->latest = *data;
    scr->has_latest = true;

    const bool connected = data->connected;
    const bool map_ok = connected && isfinite(data->map_sensor_kpa);
    const bool metric = !imperial(scr->units);
    char text[16];

    amber_kit_format(text, sizeof(text), map_ok, metric ? "%.0f" : "%.1f",
                     metric ? data->map_sensor_kpa
                            : data->map_sensor_kpa * 0.145038f);
    amber_page_set_hero(&scr->page, text, map_ok);

    const float intake = data->intake_air_temp;
    const struct {
        float value;
        const char *format;
    } cells[AMBER_PAGE_CELLS] = {
        {data->throttle, "%.0f"},
        {data->throttle_pot_voltage, "%.2f"},
        {metric ? intake : intake * 9.0f / 5.0f + 32.0f, "%.0f"},
        {data->rpm, "%.0f"},
    };
    for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
        const bool ok = connected && isfinite(cells[i].value);
        amber_kit_format(text, sizeof(text), ok, cells[i].format,
                         cells[i].value);
        amber_page_set_cell(&scr->page, i, text, ok);
    }

    // L'état de charge se déduit de l'ouverture papillon déjà affichée.
    const char *status_text;
    if (!connected) {
        status_text = "ECU NON CONNECTEE";
    } else if (isnan(data->throttle)) {
        status_text = "PAPILLON --";
    } else if (data->throttle < THROTTLE_IDLE_MAX_PCT) {
        status_text = "PIED LEVE";
    } else if (data->throttle >= THROTTLE_FULL_MIN_PCT) {
        status_text = "PLEINE CHARGE";
    } else {
        status_text = "CHARGE PARTIELLE";
    }
    amber_page_set_status(&scr->page, status_text, false);
}

void admission_screen_destroy(admission_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    lv_free(scr);
}
