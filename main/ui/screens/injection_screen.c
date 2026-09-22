#include "ui/screens/injection_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

// Repère de conception commun aux écrans ambre : 320 unités sur le diamètre.
#define SCREEN_CX 160.0f
#define COLUMN_LEFT  100.0f
#define COLUMN_RIGHT 220.0f
#define LABEL_WIDTH  116.0f
#define VALUE_WIDTH  112.0f
#define SECONDARY_SCALE 288 // 1.125x, LVGL transform scale uses 256 = 1.0x.
#define HEADER_Y             26.0f
#define SUBHEADER_Y          49.0f
#define HERO_Y               91.0f
#define HERO_LABEL_Y        130.0f
#define TRIM_LABEL_Y        177.0f
#define TRIM_VALUE_Y        199.0f
#define INJ_LABEL_Y         244.0f
#define INJ_VALUE_Y         267.0f

#define METRIC_COUNT 4
enum {
    METRIC_SHORT_TRIM = 0,
    METRIC_LONG_TRIM,
    METRIC_INJECTOR_1,
    METRIC_INJECTOR_2
};

struct injection_screen_s {
    lv_obj_t *root;
    lv_obj_t *hero_front;
    lv_obj_t *hero_shadow;
    lv_obj_t *metric_front[METRIC_COUNT];
    lv_obj_t *metric_shadow[METRIC_COUNT];
    bool connected;
    char hero_text[16];
    char metric_text[METRIC_COUNT][20];
};

// Le cadran reste volontairement ouvert : aucune couronne, aucun arc et
// aucune grille ne vient concurrencer la correction centrale ou les deux
// colonnes de mesures. La géométrie des labels fournit la symétrie à toutes
// les tailles de canvas via amber_ui_place_centered().

static bool set_pair_text_if_changed(char *buffer, size_t buffer_size,
                                     lv_obj_t *front, lv_obj_t *shadow,
                                     const char *text) {
    if (strcmp(buffer, text) == 0) return false;
    snprintf(buffer, buffer_size, "%s", text);
    lv_label_set_text_static(front, buffer);
    lv_label_set_text_static(shadow, buffer);
    return true;
}

static bool set_pair_value_if_changed(char *buffer, size_t buffer_size,
                                      lv_obj_t *front, lv_obj_t *shadow,
                                      const char *format, float value) {
    char next[24];
    if (isfinite(value)) snprintf(next, sizeof(next), format, value);
    else snprintf(next, sizeof(next), "--");
    return set_pair_text_if_changed(buffer, buffer_size, front, shadow, next);
}

injection_screen_t *injection_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    injection_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    scr->root = amber_ui_root_create(parent);
    if (scr->root == NULL) goto fail;

    const lv_font_t *font_xl = amber_ui_font_hero();
    const lv_font_t *font_l = amber_ui_font_value();
    const lv_font_t *font_m = amber_ui_font_caption();
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    lv_obj_t *header = amber_ui_label_create(scr->root, font_m, dim,
                                              "INJECTION", 180.0f);
    lv_obj_t *subheader = amber_ui_label_create(scr->root, font_m, dim,
                                                "", 220.0f);
    lv_obj_t *hero_label = amber_ui_label_create(scr->root, font_m, dim,
                                                 "CORRECTION", 150.0f);
    lv_obj_t *trim_short_label = amber_ui_label_create(
        scr->root, font_m, dim, "TRIM COURT", 130.0f);
    lv_obj_t *trim_long_label = amber_ui_label_create(
        scr->root, font_m, dim, "TRIM LONG", 130.0f);
    lv_obj_t *inj_one_label = amber_ui_label_create(
        scr->root, font_m, dim, "INJECT. 1", 130.0f);
    lv_obj_t *inj_two_label = amber_ui_label_create(
        scr->root, font_m, dim, "INJECT. 2", 130.0f);

    strcpy(scr->hero_text, "--");
    for (int i = 0; i < METRIC_COUNT; i++) {
        strcpy(scr->metric_text[i], "--");
    }
    scr->hero_front = amber_ui_label_create(scr->root, font_xl, bright,
                                             scr->hero_text, 190.0f);
    scr->hero_shadow = amber_ui_label_create(scr->root, font_xl,
                                              ui_theme_amber_separator(),
                                              scr->hero_text, 190.0f);
    for (int i = 0; i < METRIC_COUNT; i++) {
        scr->metric_front[i] = amber_ui_label_create(
            scr->root, font_l, bright, scr->metric_text[i], VALUE_WIDTH);
        scr->metric_shadow[i] = amber_ui_label_create(
            scr->root, font_l, ui_theme_amber_separator(), scr->metric_text[i],
            VALUE_WIDTH);
    }

    if (header == NULL || subheader == NULL || hero_label == NULL ||
        trim_short_label == NULL || trim_long_label == NULL ||
        inj_one_label == NULL || inj_two_label == NULL ||
        scr->hero_front == NULL || scr->hero_shadow == NULL) goto fail;
    for (int i = 0; i < METRIC_COUNT; i++) {
        if (scr->metric_front[i] == NULL || scr->metric_shadow[i] == NULL) goto fail;
        lv_obj_add_flag(scr->metric_shadow[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_transform_scale(scr->metric_front[i],
                                         SECONDARY_SCALE, 0);
        lv_obj_set_style_transform_scale(scr->metric_shadow[i],
                                         SECONDARY_SCALE, 0);
    }
    lv_obj_add_flag(scr->hero_shadow, LV_OBJ_FLAG_HIDDEN);

    amber_ui_place_centered(header, scr->root, SCREEN_CX, HEADER_Y,
                            180.0f, 0.0f);
    amber_ui_place_centered(subheader, scr->root, SCREEN_CX, SUBHEADER_Y,
                            220.0f, 0.0f);
    amber_ui_place_centered(scr->hero_shadow, scr->root, SCREEN_CX, HERO_Y,
                            190.0f, amber_ui_bold_spread(2));
    amber_ui_place_centered(scr->hero_front, scr->root, SCREEN_CX, HERO_Y,
                            190.0f, -amber_ui_bold_spread(2));
    amber_ui_place_centered(hero_label, scr->root, SCREEN_CX, HERO_LABEL_Y,
                            150.0f, 0.0f);

    amber_ui_place_centered(trim_short_label, scr->root, COLUMN_LEFT,
                            TRIM_LABEL_Y, LABEL_WIDTH, 0.0f);
    amber_ui_place_centered(trim_long_label, scr->root, COLUMN_RIGHT,
                            TRIM_LABEL_Y, LABEL_WIDTH, 0.0f);
    amber_ui_place_centered(scr->metric_shadow[METRIC_SHORT_TRIM], scr->root,
                            COLUMN_LEFT, TRIM_VALUE_Y, VALUE_WIDTH,
                            amber_ui_bold_spread(1));
    amber_ui_place_centered(scr->metric_front[METRIC_SHORT_TRIM], scr->root,
                            COLUMN_LEFT, TRIM_VALUE_Y, VALUE_WIDTH,
                            -amber_ui_bold_spread(1));
    amber_ui_place_centered(scr->metric_shadow[METRIC_LONG_TRIM], scr->root,
                            COLUMN_RIGHT, TRIM_VALUE_Y, VALUE_WIDTH,
                            amber_ui_bold_spread(1));
    amber_ui_place_centered(scr->metric_front[METRIC_LONG_TRIM], scr->root,
                            COLUMN_RIGHT, TRIM_VALUE_Y, VALUE_WIDTH,
                            -amber_ui_bold_spread(1));

    amber_ui_place_centered(inj_one_label, scr->root, COLUMN_LEFT,
                            INJ_LABEL_Y, LABEL_WIDTH, 0.0f);
    amber_ui_place_centered(inj_two_label, scr->root, COLUMN_RIGHT,
                            INJ_LABEL_Y, LABEL_WIDTH, 0.0f);
    amber_ui_place_centered(scr->metric_shadow[METRIC_INJECTOR_1], scr->root,
                            COLUMN_LEFT, INJ_VALUE_Y, VALUE_WIDTH,
                            amber_ui_bold_spread(1));
    amber_ui_place_centered(scr->metric_front[METRIC_INJECTOR_1], scr->root,
                            COLUMN_LEFT, INJ_VALUE_Y, VALUE_WIDTH,
                            -amber_ui_bold_spread(1));
    amber_ui_place_centered(scr->metric_shadow[METRIC_INJECTOR_2], scr->root,
                            COLUMN_RIGHT, INJ_VALUE_Y, VALUE_WIDTH,
                            amber_ui_bold_spread(1));
    amber_ui_place_centered(scr->metric_front[METRIC_INJECTOR_2], scr->root,
                            COLUMN_RIGHT, INJ_VALUE_Y, VALUE_WIDTH,
                            -amber_ui_bold_spread(1));

    return scr;

fail:
    injection_screen_destroy(scr);
    return NULL;
}

void injection_screen_update(injection_screen_t *scr, const ecu_data_t *data) {
    if (scr == NULL || data == NULL) return;

    if (data->connected) {
        set_pair_value_if_changed(
            scr->hero_text, sizeof(scr->hero_text), scr->hero_front,
            scr->hero_shadow, "%.0f%%", data->fuelling_feedback_percent);
        set_pair_value_if_changed(
            scr->metric_text[METRIC_SHORT_TRIM],
            sizeof(scr->metric_text[METRIC_SHORT_TRIM]),
            scr->metric_front[METRIC_SHORT_TRIM],
            scr->metric_shadow[METRIC_SHORT_TRIM], "%+.1f%%",
            data->short_term_trim_percent);
        set_pair_value_if_changed(
            scr->metric_text[METRIC_LONG_TRIM],
            sizeof(scr->metric_text[METRIC_LONG_TRIM]),
            scr->metric_front[METRIC_LONG_TRIM],
            scr->metric_shadow[METRIC_LONG_TRIM], "%+.1f%%",
            data->long_term_trim);
        set_pair_value_if_changed(
            scr->metric_text[METRIC_INJECTOR_1],
            sizeof(scr->metric_text[METRIC_INJECTOR_1]),
            scr->metric_front[METRIC_INJECTOR_1],
            scr->metric_shadow[METRIC_INJECTOR_1], "%.2f ms",
            data->injector_1_pw);
        set_pair_value_if_changed(
            scr->metric_text[METRIC_INJECTOR_2],
            sizeof(scr->metric_text[METRIC_INJECTOR_2]),
            scr->metric_front[METRIC_INJECTOR_2],
            scr->metric_shadow[METRIC_INJECTOR_2], "%.2f ms",
            data->injector_2_pw);
    } else {
        set_pair_text_if_changed(
            scr->hero_text, sizeof(scr->hero_text), scr->hero_front,
            scr->hero_shadow, "--");
        for (int i = 0; i < METRIC_COUNT; i++) {
            set_pair_text_if_changed(
                scr->metric_text[i], sizeof(scr->metric_text[i]),
                scr->metric_front[i], scr->metric_shadow[i], "--");
        }
    }

    scr->connected = data->connected;
    // Les buffers persistants et set_text_static évitent les allocations ; les
    // setters ne sont appelés que si la chaîne affichée a changé.
}

void injection_screen_destroy(injection_screen_t *scr) {
    if (scr == NULL) return;
    if (scr->root != NULL) lv_obj_delete(scr->root);
    lv_free(scr);
}
