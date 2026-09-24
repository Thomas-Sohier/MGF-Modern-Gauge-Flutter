#include "ui/screens/lambda_screen.h"

#include "ui/widgets/amber_kit.h"

#include <math.h>
#include <stdio.h>

// AFR comparé à la stœchiométrie : hors de la bande, la légende signale un
// mélange riche ou pauvre.
#define AFR_STOICH       14.7f
#define AFR_BAND          0.3f

enum { CELL_O2_LEFT = 0, CELL_O2_RIGHT, CELL_LAMBDA, CELL_HEATER };

struct lambda_screen_s {
    amber_page_t page;
};

lambda_screen_t *lambda_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    lambda_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    const amber_page_spec_t spec = {
        .title = "RICHESSE",
        .hero_unit = "AFR",
        .hero_caption = "AIR / CARBURANT",
        .cell_caption = {"O2 GAUCHE", "O2 DROITE", "LAMBDA", "CHAUFFAGE"},
        .cell_unit = {"mV", "mV", "", "%"},
        .rim = AMBER_RIM_HAIRLINE,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) {
        lambda_screen_destroy(scr);
        return NULL;
    }
    amber_page_set_status(&scr->page, "PAS DE LIAISON", false);
    return scr;
}

void lambda_screen_update(lambda_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL) return;
    const bool connected = data->connected;
    const float afr = data->estimated_air_fuel;
    const bool afr_ok = connected && isfinite(afr);
    char text[16];

    amber_kit_format(text, sizeof(text), afr_ok, "%.2f", afr);
    amber_page_set_hero(&scr->page, text, afr_ok);

    const bool left_ok = connected && isfinite(data->lambda_mv);
    const bool right_ok = connected && isfinite(data->o2_mv);
    const bool duty_ok = connected && isfinite(data->lambda_sensor_duty_cycle);
    amber_kit_format(text, sizeof(text), left_ok, "%.0f", data->lambda_mv);
    amber_page_set_cell(&scr->page, CELL_O2_LEFT, text, left_ok);
    amber_kit_format(text, sizeof(text), right_ok, "%.0f", data->o2_mv);
    amber_page_set_cell(&scr->page, CELL_O2_RIGHT, text, right_ok);
    amber_kit_format(text, sizeof(text), afr_ok, "%.2f", afr / AFR_STOICH);
    amber_page_set_cell(&scr->page, CELL_LAMBDA, text, afr_ok);
    amber_kit_format(text, sizeof(text), duty_ok, "%.0f",
                     data->lambda_sensor_duty_cycle);
    amber_page_set_cell(&scr->page, CELL_HEATER, text, duty_ok);

    if (!connected) {
        amber_page_set_hero_caption(&scr->page, "AIR / CARBURANT", false);
        amber_page_set_status(&scr->page, "PAS DE LIAISON", false);
        return;
    }
    if (!afr_ok) amber_page_set_hero_caption(&scr->page, "AIR / CARBURANT", false);
    else if (afr < AFR_STOICH - AFR_BAND) {
        amber_page_set_hero_caption(&scr->page, "MELANGE RICHE", true);
    } else if (afr > AFR_STOICH + AFR_BAND) {
        amber_page_set_hero_caption(&scr->page, "MELANGE PAUVRE", true);
    } else {
        amber_page_set_hero_caption(&scr->page, "STOECHIOMETRIE", false);
    }
    const bool heating = duty_ok && data->lambda_sensor_duty_cycle > 0.0f;
    amber_page_set_status(&scr->page,
                          !duty_ok ? "CHAUFFAGE INDISPONIBLE"
                          : heating ? "CHAUFFAGE ACTIF" : "CHAUFFAGE INACTIF",
                          false);
}

void lambda_screen_destroy(lambda_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    lv_free(scr);
}
