#include "ui/screens/ignition_screen.h"

#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"

#include <math.h>
#include <stdio.h>

// Avance à l'allumage de -10° à +50°, repère au PMH (0°) : la couronne se
// remplit depuis ce repère, en arrière pour un retard.
#define ADVANCE_MIN (-10.0f)
#define ADVANCE_MAX 50.0f

enum { CELL_OFFSET = 0, CELL_DWELL, CELL_COIL_1, CELL_COIL_2 };

struct ignition_screen_s {
    amber_page_t page;
};

ignition_screen_t *ignition_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    ignition_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    const amber_page_spec_t spec = {
        .title = "ALLUMAGE",
        .hero_unit = "°",
        .hero_caption = "AVANCE AV. PMH",
        .cell_caption = {"CORRECTION", "CHARGE", "BOBINE 1", "BOBINE 2"},
        .cell_unit = {"°", "ms", "ms", "ms"},
        .rim = AMBER_RIM_SEGMENTS,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) {
        ignition_screen_destroy(scr);
        return NULL;
    }
    const float tdc = amber_progress(0.0f, ADVANCE_MIN, ADVANCE_MAX);
    amber_page_set_rim(&scr->page, tdc, tdc, tdc, false);
    amber_page_set_status(&scr->page, "PAS DE LIAISON", false);
    return scr;
}

void ignition_screen_update(ignition_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL) return;
    const bool connected = data->connected;
    const float advance = data->ignition_advance;
    const bool advance_ok = connected && isfinite(advance);
    char text[24];

    amber_kit_format(text, sizeof(text), advance_ok, "%.1f", advance);
    amber_page_set_hero(&scr->page, text, advance_ok);
    const float tdc = amber_progress(0.0f, ADVANCE_MIN, ADVANCE_MAX);
    amber_page_set_rim(
        &scr->page, tdc,
        advance_ok ? amber_progress(advance, ADVANCE_MIN, ADVANCE_MAX) : tdc,
        tdc, advance_ok);

    const struct {
        float value;
        const char *format;
    } cells[AMBER_PAGE_CELLS] = {
        {data->ignition_advance_offset, "%+.1f"},
        {data->coil_time_microseconds / 1000.0f, "%.2f"},
        {data->coil_1_charge_time, "%.2f"},
        {data->coil_2_charge_time, "%.2f"},
    };
    for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
        const bool ok = connected && isfinite(cells[i].value);
        amber_kit_format(text, sizeof(text), ok, cells[i].format,
                         cells[i].value);
        amber_page_set_cell(&scr->page, i, text, ok);
    }

    // Le régime donne le contexte de lecture de l'avance.
    if (!connected) {
        amber_page_set_status(&scr->page, "PAS DE LIAISON", false);
    } else if (isfinite(data->rpm)) {
        snprintf(text, sizeof(text), "A %.0f TR/MIN", data->rpm);
        amber_page_set_status(&scr->page, text, false);
    } else {
        amber_page_set_status(&scr->page, "REGIME INCONNU", false);
    }
}

void ignition_screen_destroy(ignition_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    lv_free(scr);
}
