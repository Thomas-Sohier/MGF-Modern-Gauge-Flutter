#include "ui/widgets/amber_text.h"

#include "ui/widgets/amber_ui.h"

bool amber_text_create(amber_text_t *text, lv_obj_t *parent,
                       const lv_font_t *font, lv_color_t front_color,
                       lv_color_t shadow_color, const char *initial,
                       float x, float y, float width, float spread) {
    if (text == NULL) return false;
    *text = (amber_text_t){
        .parent = parent, .x = x, .y = y, .width = width, .spread = spread,
    };
    text->shadow = amber_ui_label_create(parent, font, shadow_color, initial,
                                          width);
    if (text->shadow == NULL) return false;
    text->front = amber_ui_label_create(parent, font, front_color, initial,
                                         width);
    if (text->front == NULL) {
        lv_obj_delete(text->shadow);
        text->shadow = NULL;
        return false;
    }
    amber_text_place(text);
    return true;
}

bool amber_text_valid(const amber_text_t *text) {
    return text != NULL && text->front != NULL && text->shadow != NULL;
}

void amber_text_place(amber_text_t *text) {
    if (!amber_text_valid(text)) return;
    amber_ui_place_centered(text->shadow, text->parent, text->x, text->y,
                            text->width, text->spread);
    amber_ui_place_centered(text->front, text->parent, text->x, text->y,
                            text->width, -text->spread);
}

void amber_text_set_static(amber_text_t *text, const char *value) {
    if (!amber_text_valid(text)) return;
    const char *safe = value != NULL ? value : "";
    lv_label_set_text_static(text->front, safe);
    lv_label_set_text_static(text->shadow, safe);
    amber_text_place(text);
}

void amber_text_set_colors(amber_text_t *text, lv_color_t front,
                           lv_color_t shadow) {
    if (!amber_text_valid(text)) return;
    lv_obj_set_style_text_color(text->front, front, 0);
    lv_obj_set_style_text_color(text->shadow, shadow, 0);
}

void amber_text_clear(amber_text_t *text) {
    if (text == NULL) return;
    *text = (amber_text_t){0};
}
