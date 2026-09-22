#include "ui/screens/admission_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>

// Repère de conception commun au cadran ambre : 320 unités inscrites dans le
// plus grand carré disponible. Le panneau physique est un disque de 480 px.
#define CX 160.0f
#define CY 160.0f

#define MAP_MAX             200.0f
#define MAP_RING_R          148.0f
#define MAP_RING_W            3.5f
#define MAP_RING_SEGMENTS    16
#define MAP_RING_GAP          3.0f
#define MAP_RING_START      190.0f
#define MAP_RING_SWEEP       160.0f
#define MAP_TICK_COUNT        5

// Repère compact dans le disque de 320 px : la valeur MAP occupe le centre,
// un seul repère de pression l'encadre, puis quatre mesures sont regroupées
// dans une grille ouverte. Chaque ligne reste à distance de la circonférence.
#define HEADER_Y             25.0f
#define STATUS_Y             46.0f
#define HEADER_LINE_Y        57.0f

#define HERO_Y               91.0f
#define HERO_UNIT_Y         123.0f
#define HERO_LABEL_Y        139.0f

#define AIRFLOW_CY          166.0f
#define AIRFLOW_LINE_W        1.0f
#define AIRFLOW_ICON_W        1.8f

#define MATRIX_LINE_Y        201.0f
#define MATRIX_SPLIT_Y       249.0f
#define MATRIX_BOTTOM_Y      294.0f
#define VALUE_TOP_Y          217.0f
#define VALUE_BOTTOM_Y       264.0f
#define LABEL_TOP_Y          238.0f
#define LABEL_BOTTOM_Y       285.0f

#define METRIC_COUNT 4
enum {
    METRIC_THROTTLE = 0,
    METRIC_TPS,
    METRIC_INTAKE,
    METRIC_MAP_AUX
};

// Les quatre indicateurs restent lisibles dans deux colonnes centrées. MAP est
// répété en bas pour conserver le contexte de la grille sans concurrencer la
// lecture héroïque centrale.
static const float kMetricX[METRIC_COUNT] = {96.0f, 224.0f, 96.0f, 224.0f};
static const float kMetricY[METRIC_COUNT] = {
    VALUE_TOP_Y, VALUE_TOP_Y, VALUE_BOTTOM_Y, VALUE_BOTTOM_Y
};
static const float kMetricLabelY[METRIC_COUNT] = {
    LABEL_TOP_Y, LABEL_TOP_Y, LABEL_BOTTOM_Y, LABEL_BOTTOM_Y
};

// Une paire de labels permet le faux-gras Michroma du style amber sans
// allocation ou fonte bold supplémentaire.
typedef struct {
    lv_obj_t *front;
    lv_obj_t *shadow;
    float x;
    float y;
    int32_t width;
    int spread;
} amber_text_t;

struct admission_screen_s {
    lv_obj_t *root;
    lv_obj_t *canvas;

    lv_obj_t *header;
    lv_obj_t *status;
    lv_obj_t *hero_unit;
    lv_obj_t *hero_label;
    lv_obj_t *metric_label[METRIC_COUNT];

    amber_text_t hero;
    amber_text_t metric[METRIC_COUNT];

    char hero_text[16];
    char metric_text[METRIC_COUNT][20];
    char status_text[24];

    bool connected;
    float map_kpa;
    float throttle;
    float throttle_pot_voltage;
    float intake_air_temp;
};

static void draw_map_ring(lv_layer_t *layer, const ui_layout_t *layout,
                          const admission_screen_t *scr) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();
    const float progress = scr->connected
        ? amber_clampf(scr->map_kpa / MAP_MAX, 0.0f, 1.0f) : 0.0f;
    const float bar = (MAP_RING_SWEEP -
                       (MAP_RING_SEGMENTS - 1) * MAP_RING_GAP) /
                      MAP_RING_SEGMENTS;
    const float pitch = bar + MAP_RING_GAP;
    const int lit = (int)lroundf(progress * MAP_RING_SEGMENTS);

    // Une couronne courte et discrète donne une échelle sans enfermer la
    // lecture centrale. Les extrémités et graduations restent dans la marge
    // circulaire sûre du panneau.
    for (int i = 0; i < MAP_RING_SEGMENTS; i++) {
        const float start = MAP_RING_START + (float)i * pitch;
        amber_draw_arc_wrapped(layer, layout, CX, CY, MAP_RING_R, MAP_RING_W,
                                start, bar, i < lit ? bright : dim, false);
    }

    for (int i = 0; i <= MAP_TICK_COUNT; i++) {
        const float angle = MAP_RING_START + MAP_RING_SWEEP *
                            (float)i / MAP_TICK_COUNT;
        amber_draw_tick(layer, layout, CX, CY, angle, MAP_RING_R - 3.0f,
                        MAP_RING_R + 2.0f, i == 0 || i == MAP_TICK_COUNT
                        ? 1.0f : 0.7f, separator, false);
    }
}

static void draw_flow_arrow(lv_layer_t *layer, const ui_layout_t *layout,
                            float x, float y, lv_color_t color) {
    amber_draw_line(layer, layout, x - 5.0f, y - 3.0f, x, y, AIRFLOW_ICON_W,
                    color, true);
    amber_draw_line(layer, layout, x, y, x - 5.0f, y + 3.0f, AIRFLOW_ICON_W,
                    color, true);
}

static void draw_airflow_gauge(lv_layer_t *layer, const ui_layout_t *layout,
                               const admission_screen_t *scr) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();
    const lv_color_t flow = scr->connected && scr->throttle > 1.0f
                                ? bright : dim;
    const float throttle = scr->connected
        ? amber_clampf(scr->throttle / 100.0f, 0.0f, 1.0f) : 0.0f;
    const float map_progress = scr->connected
        ? amber_clampf(scr->map_kpa / MAP_MAX, 0.0f, 1.0f) : 0.0f;
    const float butterfly_x = 95.0f;
    const float bar_left = 124.0f;
    const float bar_right = 271.0f;
    const float bar_fill = bar_left + (bar_right - bar_left) * map_progress;

    // Pictogramme compact : arrivée d'air, papillon, puis une seule barre
    // horizontale de pression. Il remplace le conduit détaillé sans perdre le
    // sens du flux ni le lien visuel avec la valeur MAP centrale.
    amber_draw_line(layer, layout, 48.0f, AIRFLOW_CY, 82.0f, AIRFLOW_CY,
                    AIRFLOW_LINE_W, flow, true);
    draw_flow_arrow(layer, layout, 73.0f, AIRFLOW_CY, flow);

    amber_draw_circle(layer, layout, butterfly_x, AIRFLOW_CY, 12.0f,
                      AIRFLOW_LINE_W, separator);
    const float plate_dx = 3.0f + throttle * 5.0f;
    amber_draw_line(layer, layout, butterfly_x - plate_dx, AIRFLOW_CY + 8.0f,
                    butterfly_x + plate_dx, AIRFLOW_CY - 8.0f, 2.0f, flow,
                    true);
    amber_draw_dot(layer, layout, butterfly_x, AIRFLOW_CY, 2.0f, bright);

    amber_draw_line(layer, layout, 114.0f, AIRFLOW_CY, bar_right,
                    AIRFLOW_CY, 2.0f, dim, true);
    if (scr->connected) {
        amber_draw_line(layer, layout, bar_left, AIRFLOW_CY, bar_fill,
                        AIRFLOW_CY, AIRFLOW_LINE_W + 1.0f, bright, true);
    }
    for (int i = 0; i <= 4; i++) {
        const float x = bar_left + (bar_right - bar_left) * (float)i / 4.0f;
        amber_draw_line(layer, layout, x, AIRFLOW_CY - 4.0f, x,
                        AIRFLOW_CY + 4.0f, AIRFLOW_LINE_W, separator, false);
    }
}

static void canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    admission_screen_t *scr = lv_obj_get_user_data(canvas);
    if (layer == NULL || scr == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);
    const lv_color_t separator = ui_theme_amber_separator();

    draw_map_ring(layer, &layout, scr);
    amber_draw_line(layer, &layout, 38.0f, HEADER_LINE_Y, 128.0f,
                    HEADER_LINE_Y, AIRFLOW_LINE_W, separator, false);
    amber_draw_line(layer, &layout, 192.0f, HEADER_LINE_Y, 282.0f,
                    HEADER_LINE_Y, AIRFLOW_LINE_W, separator, false);
    draw_airflow_gauge(layer, &layout, scr);

    // Grille ouverte et légère : les ruptures autour du croisement évitent de
    // couper les valeurs, tout en donnant quatre cellules immédiatement
    // repérables à la lecture.
    amber_draw_line(layer, &layout, 43.0f, MATRIX_LINE_Y, 143.0f,
                    MATRIX_LINE_Y, AIRFLOW_LINE_W, separator, false);
    amber_draw_line(layer, &layout, 177.0f, MATRIX_LINE_Y, 277.0f,
                    MATRIX_LINE_Y, AIRFLOW_LINE_W, separator, false);
    amber_draw_line(layer, &layout, 43.0f, MATRIX_SPLIT_Y, 143.0f,
                    MATRIX_SPLIT_Y, AIRFLOW_LINE_W, separator, false);
    amber_draw_line(layer, &layout, 177.0f, MATRIX_SPLIT_Y, 277.0f,
                    MATRIX_SPLIT_Y, AIRFLOW_LINE_W, separator, false);
    amber_draw_line(layer, &layout, 160.0f, MATRIX_LINE_Y + 5.0f, 160.0f,
                    MATRIX_SPLIT_Y - 5.0f, AIRFLOW_LINE_W, separator, false);
    amber_draw_line(layer, &layout, 160.0f, MATRIX_SPLIT_Y + 5.0f, 160.0f,
                    MATRIX_BOTTOM_Y - 4.0f, AIRFLOW_LINE_W, separator, false);
}

static amber_text_t text_create(lv_obj_t *parent, const lv_font_t *font,
                                float x, float y, int32_t width, int spread,
                                const char *initial) {
    amber_text_t text = {
        .x = x, .y = y, .width = width, .spread = spread
    };
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t separator = ui_theme_amber_separator();

    text.shadow = amber_ui_label_create(parent, font, separator, initial,
                                        (float)width);
    if (text.shadow == NULL) return text;
    text.front = amber_ui_label_create(parent, font, bright, initial,
                                       (float)width);
    if (text.front == NULL) {
        lv_obj_delete(text.shadow);
        text.shadow = NULL;
        return text;
    }
    lv_obj_add_flag(text.shadow, LV_OBJ_FLAG_HIDDEN);
    const float offset = (float)spread * amber_ui_bold_spread(1);
    amber_ui_place_centered(text.shadow, parent, x, y, (float)width, offset);
    amber_ui_place_centered(text.front, parent, x, y, (float)width, -offset);
    return text;
}

static bool text_valid(const amber_text_t *text) {
    return text != NULL && text->front != NULL && text->shadow != NULL;
}

static void text_set(amber_text_t *text, lv_obj_t *parent, const char *value) {
    if (!text_valid(text)) return;
    lv_label_set_text_static(text->front, value != NULL ? value : "");
    lv_label_set_text_static(text->shadow, value != NULL ? value : "");
    const float offset = (float)text->spread * UI_REFERENCE_SIZE /
                         UI_DISPLAY_SIZE_PX;
    amber_ui_place_centered(text->shadow, parent, text->x, text->y,
                            (float)text->width, offset);
    amber_ui_place_centered(text->front, parent, text->x, text->y,
                            (float)text->width, -offset);
}

static void text_destroy(amber_text_t *text) {
    if (text == NULL) return;
    // Les deux labels sont enfants de root ; root les supprimera en bloc.
    text->front = NULL;
    text->shadow = NULL;
}

static void set_metric_text(admission_screen_t *scr, int index,
                            const char *format, float value) {
    snprintf(scr->metric_text[index], sizeof(scr->metric_text[index]),
             format, value);
}

static void set_unavailable(admission_screen_t *scr) {
    snprintf(scr->hero_text, sizeof(scr->hero_text), "--");
    for (int i = 0; i < METRIC_COUNT; i++) {
        snprintf(scr->metric_text[i], sizeof(scr->metric_text[i]), "--");
    }
}

admission_screen_t *admission_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    admission_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    scr->root = amber_ui_root_create(parent);
    if (scr->root == NULL) goto fail;
    scr->canvas = amber_ui_canvas_create(scr->root, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    snprintf(scr->hero_text, sizeof(scr->hero_text), "--");
    snprintf(scr->status_text, sizeof(scr->status_text), "HORS LIGNE");
    for (int i = 0; i < METRIC_COUNT; i++) {
        snprintf(scr->metric_text[i], sizeof(scr->metric_text[i]), "--");
    }

    const lv_font_t *hero_font = ui_font_or(ui_font_xl,
                                            &lv_font_montserrat_48);
    const lv_font_t *value_font = ui_font_or(ui_font_l,
                                             &lv_font_montserrat_20);
    const lv_font_t *caption_font = ui_font_or(ui_font_m,
                                               &lv_font_montserrat_14);
    const lv_color_t bright = ui_theme_amber_bright();

    scr->header = amber_ui_label_create(scr->root, value_font, bright,
                                        "ADMISSION", 180.0f);
    scr->status = amber_ui_label_create(scr->root, caption_font, bright,
                                        scr->status_text, 220.0f);
    scr->hero = text_create(scr->root, hero_font, CX, HERO_Y, 170, 2,
                            scr->hero_text);
    scr->hero_unit = amber_ui_label_create(scr->root, value_font, bright,
                                           "kPa", 70.0f);
    scr->hero_label = amber_ui_label_create(scr->root, value_font, bright,
                                            "PRESSION MAP", 190.0f);

    static const char *const kMetricLabels[METRIC_COUNT] = {
        "PAPILLON", "TPS", "AIR", "MAP"
    };
    for (int i = 0; i < METRIC_COUNT; i++) {
        scr->metric[i] = text_create(scr->root, value_font, kMetricX[i],
                                     kMetricY[i], 118, 1,
                                     scr->metric_text[i]);
        scr->metric_label[i] = amber_ui_label_create(
            scr->root, value_font, bright, kMetricLabels[i], 112.0f);
    }

    if (scr->header == NULL || scr->status == NULL ||
        !text_valid(&scr->hero) || scr->hero_unit == NULL ||
        scr->hero_label == NULL) goto fail;
    for (int i = 0; i < METRIC_COUNT; i++) {
        if (!text_valid(&scr->metric[i]) || scr->metric_label[i] == NULL) {
            goto fail;
        }
    }

    amber_ui_place_centered(scr->header, scr->root, CX, HEADER_Y, 180.0f,
                             0.0f);
    amber_ui_place_centered(scr->status, scr->root, CX, STATUS_Y, 220.0f,
                             0.0f);
    amber_ui_place_centered(scr->hero.shadow, scr->root, CX, HERO_Y, 170.0f,
                             amber_ui_bold_spread(2));
    amber_ui_place_centered(scr->hero.front, scr->root, CX, HERO_Y, 170.0f,
                             -amber_ui_bold_spread(2));
    amber_ui_place_centered(scr->hero_unit, scr->root, CX, HERO_UNIT_Y,
                             70.0f, 0.0f);
    amber_ui_place_centered(scr->hero_label, scr->root, CX, HERO_LABEL_Y,
                             190.0f, 0.0f);
    for (int i = 0; i < METRIC_COUNT; i++) {
        const float offset = amber_ui_bold_spread(1);
        amber_ui_place_centered(scr->metric[i].shadow, scr->root,
                                kMetricX[i], kMetricY[i], 118.0f, offset);
        amber_ui_place_centered(scr->metric[i].front, scr->root,
                                kMetricX[i], kMetricY[i], 118.0f, -offset);
        amber_ui_place_centered(scr->metric_label[i], scr->root,
                                kMetricX[i], kMetricLabelY[i], 112.0f, 0.0f);
    }

    return scr;

fail:
    admission_screen_destroy(scr);
    return NULL;
}

void admission_screen_update(admission_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL || scr->canvas == NULL) return;

    scr->connected = data->connected;
    scr->map_kpa = scr->connected && isfinite(data->map_sensor_kpa)
                       ? amber_clampf(data->map_sensor_kpa, 0.0f, MAP_MAX) : 0.0f;
    scr->throttle = scr->connected && isfinite(data->throttle)
                        ? amber_clampf(data->throttle, 0.0f, 100.0f) : 0.0f;
    scr->throttle_pot_voltage = scr->connected &&
                                isfinite(data->throttle_pot_voltage)
                                    ? amber_clampf(data->throttle_pot_voltage, 0.0f,
                                             5.0f) : 0.0f;
    scr->intake_air_temp = scr->connected && isfinite(data->intake_air_temp)
                               ? data->intake_air_temp : 0.0f;

    if (!scr->connected) {
        set_unavailable(scr);
        snprintf(scr->status_text, sizeof(scr->status_text), "HORS LIGNE");
    } else {
        snprintf(scr->hero_text, sizeof(scr->hero_text), "%.0f", scr->map_kpa);
        set_metric_text(scr, METRIC_THROTTLE, "%.0f %%", scr->throttle);
        set_metric_text(scr, METRIC_TPS, "%.2f V", scr->throttle_pot_voltage);
        set_metric_text(scr, METRIC_INTAKE, "%.0f °C", scr->intake_air_temp);
        // La quatrième mesure garde la pression MAP sous forme compacte,
        // tandis que la valeur héroïque reste la lecture principale.
        set_metric_text(scr, METRIC_MAP_AUX, "%.0f kPa", scr->map_kpa);
        snprintf(scr->status_text, sizeof(scr->status_text), "MOTEUR EN LIGNE");
    }

    text_set(&scr->hero, scr->root, scr->hero_text);
    for (int i = 0; i < METRIC_COUNT; i++) {
        text_set(&scr->metric[i], scr->root, scr->metric_text[i]);
    }
    lv_label_set_text_static(scr->status, scr->status_text);
    amber_ui_place_centered(scr->status, scr->root, CX, STATUS_Y, 220.0f,
                             0.0f);

    // Les textes viennent de buffers persistants et set_text_static ne copie
    // rien : cette voie ne fait aucune allocation pendant une mise à jour.
    lv_obj_invalidate(scr->canvas);
}

void admission_screen_destroy(admission_screen_t *scr) {
    if (scr == NULL) return;
    text_destroy(&scr->hero);
    for (int i = 0; i < METRIC_COUNT; i++) text_destroy(&scr->metric[i]);
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
