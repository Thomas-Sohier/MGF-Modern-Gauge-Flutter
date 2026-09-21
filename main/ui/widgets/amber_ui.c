#include "ui/widgets/amber_ui.h"

#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"

#include <math.h>

lv_obj_t *amber_ui_root_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    lv_obj_set_style_bg_color(parent, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_update_layout(parent);

    const int32_t side = LV_MIN(lv_obj_get_content_width(parent),
                                lv_obj_get_content_height(parent));
    if (side <= 0) return NULL;

    lv_obj_t *root = lv_obj_create(parent);
    if (root == NULL) return NULL;
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, side, side);
    lv_obj_center(root);
    lv_obj_set_style_bg_color(root, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(root, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(root, true, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_update_layout(root);
    return root;
}

lv_obj_t *amber_ui_canvas_create(lv_obj_t *root, void *context,
                                 lv_event_cb_t draw_callback) {
    if (root == NULL || draw_callback == NULL) return NULL;
    lv_obj_t *canvas = lv_obj_create(root);
    if (canvas == NULL) return NULL;
    lv_obj_remove_style_all(canvas);
    lv_obj_set_size(canvas, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(canvas, context);
    lv_obj_add_event_cb(canvas, draw_callback, LV_EVENT_DRAW_MAIN, NULL);
    return canvas;
}

lv_obj_t *amber_ui_label_create(lv_obj_t *parent, const lv_font_t *font,
                                lv_color_t color, const char *text,
                                float logical_width) {
    if (parent == NULL || font == NULL || text == NULL) return NULL;
    const ui_layout_t layout = ui_layout_fit(lv_obj_get_width(parent),
                                              lv_obj_get_height(parent));
    lv_obj_t *label = lv_label_create(parent);
    if (label == NULL) return NULL;
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    if (logical_width > 0.0f) {
        lv_obj_set_width(label, (int32_t)lroundf(logical_width * layout.scale));
        lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    }
    lv_label_set_text_static(label, text);
    return label;
}

void amber_ui_place_centered(lv_obj_t *object, lv_obj_t *parent,
                             float x, float y, float logical_width,
                             float logical_x_offset) {
    if (object == NULL || parent == NULL) return;
    const ui_layout_t layout = ui_layout_fit(lv_obj_get_width(parent),
                                              lv_obj_get_height(parent));
    if (logical_width > 0.0f) {
        lv_obj_set_width(object,
                         (int32_t)lroundf(logical_width * layout.scale));
    }
    lv_obj_update_layout(object);
    lv_obj_set_pos(
        object,
        (int32_t)lroundf(ui_layout_x(&layout, x) +
                        logical_x_offset * layout.scale) -
            lv_obj_get_width(object) / 2,
        (int32_t)lroundf(ui_layout_y(&layout, y)) -
            lv_obj_get_height(object) / 2);
}
