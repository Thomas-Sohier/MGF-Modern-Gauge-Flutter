#include "ui/widgets/amber_value.h"

#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"

#include <math.h>
#include <string.h>

struct amber_value_widget_s {
    lv_obj_t *parent;
    lv_obj_t *front;
    float ref_x;
    float ref_y;
};

static lv_obj_t *new_label(lv_obj_t *parent, const lv_font_t *font,
                           const char *text) {
    if (parent == NULL || font == NULL) return NULL;

    lv_obj_t *label = lv_label_create(parent);
    if (label == NULL) return NULL;

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, ui_theme_amber_bright(), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(label, text != NULL ? text : "");
    return label;
}

static void place_centered(lv_obj_t *label, const amber_value_widget_t *value) {
    lv_obj_update_layout(label);

    const ui_layout_t layout = ui_layout_fit(lv_obj_get_width(value->parent),
                                             lv_obj_get_height(value->parent));
    const int32_t label_w = lv_obj_get_width(label);
    const int32_t label_h = lv_obj_get_height(label);

    lv_obj_align(
        label, LV_ALIGN_TOP_LEFT,
        (int32_t)lroundf(ui_layout_x(&layout, value->ref_x)) - label_w / 2,
        (int32_t)lroundf(ui_layout_y(&layout, value->ref_y)) - label_h / 2);
}

amber_value_widget_t *amber_value_widget_create(lv_obj_t *parent,
                                                const lv_font_t *font,
                                                float ref_x, float ref_y,
                                                const char *text) {
    if (parent == NULL || font == NULL) return NULL;

    amber_value_widget_t *value = lv_malloc(sizeof(*value));
    if (value == NULL) return NULL;

    value->parent = parent;
    value->ref_x = ref_x;
    value->ref_y = ref_y;
    value->front = new_label(parent, font, text);
    if (value->front == NULL) {
        lv_free(value);
        return NULL;
    }
    place_centered(value->front, value);
    return value;
}

void amber_value_widget_set(amber_value_widget_t *value, const char *text) {
    if (value == NULL || value->front == NULL) return;

    const char *safe_text = text != NULL ? text : "";
    // lv_label_set_text invalide et recalcule le label même à texte égal.
    if (strcmp(lv_label_get_text(value->front), safe_text) == 0) return;
    lv_label_set_text(value->front, safe_text);
    place_centered(value->front, value);
}

void amber_value_widget_destroy(amber_value_widget_t *value) {
    if (value == NULL) return;

    if (value->front != NULL) lv_obj_delete(value->front);
    lv_free(value);
}
