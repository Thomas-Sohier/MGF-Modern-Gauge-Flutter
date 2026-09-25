#include "ui/screens/injection_screen.h"

#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"

#include <math.h>

// Correction de richesse centrée sur 100 % : la couronne s'allume depuis le
// repère central vers la valeur, ce qui rend le sens de la correction
// (enrichissement / appauvrissement) lisible d'un coup d'œil.
#define CORRECTION_MIN     70.0f
#define CORRECTION_MAX     130.0f
#define CORRECTION_NEUTRAL 100.0f
#define NEUTRAL_BAND       2.0f

enum { CELL_SHORT_TRIM = 0, CELL_LONG_TRIM, CELL_INJECTOR_1, CELL_INJECTOR_2 };

struct injection_screen_s {
    amber_page_t page;
};

injection_screen_t *injection_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    injection_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    const amber_page_spec_t spec = {
        .title = "INJECTION",
        .hero_unit = "%",
        .hero_caption = "CORRECTION",
        .cell_caption = {"TRIM CT", "TRIM LG", "INJECT. 1", "INJECT. 2"},
        .cell_unit = {"%", "%", "ms", "ms"},
        .rim = AMBER_RIM_SEGMENTS,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) {
        injection_screen_destroy(scr);
        return NULL;
    }
    amber_page_set_rim(&scr->page, 0.5f, 0.5f, 0.5f, false);
    amber_page_set_status(&scr->page, "PAS DE LIAISON", false);
    return scr;
}

void injection_screen_update(injection_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL) return;
    const bool connected = data->connected;
    const float correction = data->fuelling_feedback_percent;
    const bool correction_ok = connected && isfinite(correction);
    char text[16];

    amber_kit_format(text, sizeof(text), correction_ok, "%.0f", correction);
    amber_page_set_hero(&scr->page, text, correction_ok);
    const float neutral =
        amber_progress(CORRECTION_NEUTRAL, CORRECTION_MIN, CORRECTION_MAX);
    amber_page_set_rim(
        &scr->page, neutral,
        correction_ok
            ? amber_progress(correction, CORRECTION_MIN, CORRECTION_MAX)
            : neutral,
        neutral, correction_ok);

    const struct {
        float value;
        const char *format;
    } cells[AMBER_PAGE_CELLS] = {
        {data->short_term_trim_percent, "%+.1f"},
        {data->long_term_trim, "%+.1f"},
        {data->injector_1_pw, "%.2f"},
        {data->injector_2_pw, "%.2f"},
    };
    for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
        const bool ok = connected && isfinite(cells[i].value);
        amber_kit_format(text, sizeof(text), ok, cells[i].format,
                         cells[i].value);
        amber_page_set_cell(&scr->page, i, text, ok);
    }

    if (!connected) {
        amber_page_set_status(&scr->page, "PAS DE LIAISON", false);
    } else if (!correction_ok) {
        amber_page_set_status(&scr->page, "CORRECTION INCONNUE", false);
    } else if (correction > CORRECTION_NEUTRAL + NEUTRAL_BAND) {
        amber_page_set_status(&scr->page, "ENRICHISSEMENT", true);
    } else if (correction < CORRECTION_NEUTRAL - NEUTRAL_BAND) {
        amber_page_set_status(&scr->page, "APPAUVRISSEMENT", true);
    } else {
        amber_page_set_status(&scr->page, "MELANGE NEUTRE", false);
    }
}

void injection_screen_destroy(injection_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    lv_free(scr);
}
