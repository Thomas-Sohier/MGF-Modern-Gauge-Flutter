#include "ui/screens/idle_screen.h"

#include "ui/widgets/amber_kit.h"

#include <math.h>
#include <stdio.h>

// Régime de ralenti en héros ; l'écart à la consigne ECU est rappelé sous la
// valeur.
#define ERROR_WARN_RPM 50.0f

enum { CELL_SETPOINT = 0, CELL_VALVE, CELL_BASE, CELL_ADJUSTER };

struct idle_screen_s {
    amber_page_t page;
};

idle_screen_t *idle_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    idle_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    const amber_page_spec_t spec = {
        .title = "RALENTI",
        .hero_unit = "RPM",
        .hero_caption = "ECART --",
        .cell_caption = {"CONSIGNE", "VANNE", "BASE", "AJUSTEUR"},
        .cell_unit = {"RPM", "%", "%", "RPM"},
        .rim = AMBER_RIM_HAIRLINE,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) {
        idle_screen_destroy(scr);
        return NULL;
    }
    amber_page_set_status(&scr->page, "PAS DE LIAISON", false);
    return scr;
}

void idle_screen_update(idle_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL) return;
    const bool connected = data->connected;
    const bool rpm_ok = connected && isfinite(data->rpm);
    char text[24];

    amber_kit_format(text, sizeof(text), rpm_ok, "%.0f", data->rpm);
    amber_page_set_hero(&scr->page, text, rpm_ok);

    const struct {
        float value;
        const char *format;
    } cells[AMBER_PAGE_CELLS] = {
        {data->idle_setpoint, "%.0f"},
        {data->idle_valve_position, "%.0f"},
        {data->idle_base_position, "%.0f"},
        {data->idle_adjuster_rpm, "%+.0f"},
    };
    for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
        const bool ok = connected && isfinite(cells[i].value);
        amber_kit_format(text, sizeof(text), ok, cells[i].format,
                         cells[i].value);
        amber_page_set_cell(&scr->page, i, text, ok);
    }

    const bool error_ok = connected && isfinite(data->idle_error);
    if (error_ok) {
        snprintf(text, sizeof(text), "ECART %+.0f RPM", data->idle_error);
    } else {
        snprintf(text, sizeof(text), "ECART --");
    }
    amber_page_set_hero_caption(&scr->page, text,
                                error_ok &&
                                    fabsf(data->idle_error) >= ERROR_WARN_RPM);
    amber_page_set_status(
        &scr->page, connected ? "BOUCLE ACTIVE" : "PAS DE LIAISON", false);
}

void idle_screen_destroy(idle_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    lv_free(scr);
}
