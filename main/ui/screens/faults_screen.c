#include "ui/screens/faults_screen.h"

#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"

#include <stdio.h>

// Chaque sonde surveillée occupe une cellule de la grille commune. Une
// cellule en défaut s'encadre d'ambre vif, comme un voyant allumé. Une ECU
// déconnectée ou sans état de défaut reste « INCONNU », jamais « OK ».
static const struct {
    uint8_t flag;
    const char *label;
} kFaults[AMBER_PAGE_CELLS] = {
    {ECU_FAULT_COOLANT_SENSOR, "CAPT. EAU"},
    {ECU_FAULT_INTAKE_AIR_SENSOR, "CAPT. AIR"},
    {ECU_FAULT_FUEL_PUMP, "POMPE"},
    {ECU_FAULT_THROTTLE_POT, "PAPILLON"},
};

struct faults_screen_s {
    amber_page_t page;
    bool has_snapshot;
    bool connected;
    bool available;
    uint8_t flags;
};

static void draw_fault_frames(lv_layer_t *layer, const ui_layout_t *layout,
                              void *context) {
    const faults_screen_t *scr = context;
    if (scr == NULL || !scr->available) return;
    static const float xs[AMBER_PAGE_CELLS] = {
        AMBER_KIT_COL_L, AMBER_KIT_COL_R, AMBER_KIT_COL_L, AMBER_KIT_COL_R};
    static const float ys[AMBER_PAGE_CELLS] = {
        (AMBER_KIT_GRID_TOP_Y + AMBER_KIT_GRID_MID_Y) * 0.5f,
        (AMBER_KIT_GRID_TOP_Y + AMBER_KIT_GRID_MID_Y) * 0.5f,
        (AMBER_KIT_GRID_MID_Y + AMBER_KIT_GRID_BOT_Y) * 0.5f + 2.0f,
        (AMBER_KIT_GRID_MID_Y + AMBER_KIT_GRID_BOT_Y) * 0.5f + 2.0f};
    const lv_color_t on = ui_theme_amber_bright();
    for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
        if ((scr->flags & kFaults[i].flag) == 0) continue;
        const float x0 = xs[i] - 53.0f, x1 = xs[i] + 53.0f;
        const float y0 = ys[i] - 23.0f, y1 = ys[i] + 23.0f;
        const float c = 6.0f; // coins chanfreinés, écho du badge de démarrage
        const float pts[][2] = {{x0 + c, y0}, {x1 - c, y0}, {x1, y0 + c},
                                {x1, y1 - c}, {x1 - c, y1}, {x0 + c, y1},
                                {x0, y1 - c}, {x0, y0 + c}, {x0 + c, y0}};
        for (int p = 0; p + 1 < 9; p++) {
            amber_draw_line(layer, layout, pts[p][0], pts[p][1], pts[p + 1][0],
                            pts[p + 1][1], 1.6f, on, false);
        }
    }
}

faults_screen_t *faults_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    faults_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    amber_page_spec_t spec = {
        .title = "DIAGNOSTIC",
        .hero_unit = "",
        .hero_caption = "DEFAUTS INCONNUS",
        .cell_unit = {"", "", "", ""},
        .rim = AMBER_RIM_HAIRLINE,
        .cell_font = amber_kit_font_label(),
        .draw_extra = draw_fault_frames,
        .context = scr,
    };
    for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
        spec.cell_caption[i] = kFaults[i].label;
    }
    if (!amber_page_create(&scr->page, parent, &spec)) {
        faults_screen_destroy(scr);
        return NULL;
    }
    amber_page_set_status(&scr->page, "ECU NON CONNECTEE", false);
    return scr;
}

void faults_screen_update(faults_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL) return;
    const bool available = data->connected && data->faults_available;
    const uint8_t flags = available ? data->fault_flags : 0;
    if (scr->has_snapshot && scr->connected == data->connected &&
        scr->available == available && scr->flags == flags) {
        return;
    }
    scr->has_snapshot = true;
    scr->connected = data->connected;
    scr->available = available;
    scr->flags = flags;

    unsigned count = 0;
    for (int i = 0; i < AMBER_PAGE_CELLS; i++) {
        const bool fault = (flags & kFaults[i].flag) != 0;
        if (fault) count++;
        amber_page_set_cell(&scr->page, i,
                            !available ? "--"
                            : fault    ? "DEFAUT"
                                       : "OK",
                            available);
    }

    char text[8];
    if (available)
        snprintf(text, sizeof(text), "%u", count);
    else
        snprintf(text, sizeof(text), "--");
    amber_page_set_hero(&scr->page, text, available);
    amber_page_set_hero_caption(&scr->page,
                                !available   ? "DEFAUTS INCONNUS"
                                : count == 0 ? "AUCUN DEFAUT"
                                : count == 1 ? "DEFAUT ACTIF"
                                             : "DEFAUTS ACTIFS",
                                available && count > 0);
    amber_page_set_status(&scr->page,
                          !data->connected ? "ECU NON CONNECTEE"
                          : available      ? "LIAISON ACTIVE"
                                           : "CODES NON FOURNIS",
                          false);
    amber_page_invalidate(&scr->page);
}

void faults_screen_destroy(faults_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    lv_free(scr);
}
