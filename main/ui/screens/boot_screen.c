#ifndef LV_USE_VECTOR_GRAPHIC
#define LV_USE_VECTOR_GRAPHIC 1
#endif
#ifndef LV_USE_MATRIX
#define LV_USE_MATRIX 1
#endif
#ifndef LV_USE_FLOAT
#define LV_USE_FLOAT 1
#endif
#include "ui/screens/boot_screen.h"
#include "ui/themes/ui_theme.h"
#include "ui/fonts/ui_fonts.h"

#include <stdlib.h>

#define BOOT_LOGO_SIZE 240

LV_DRAW_BUF_DEFINE_STATIC(draw_buf, BOOT_LOGO_SIZE, BOOT_LOGO_SIZE, LV_COLOR_FORMAT_ARGB8888);

struct boot_screen {
    lv_obj_t *root;
    lv_obj_t *canvas;
    lv_obj_t *label;
};

static lv_vector_path_t *create_logo_path(void);

static void add_segment(lv_vector_path_t *path, float x1, float y1, float x2, float y2) {
    lv_fpoint_t a = {x1, y1};
    lv_fpoint_t b = {x2, y2};
    lv_vector_path_move_to(path, &a);
    lv_vector_path_line_to(path, &b);
}

static bool render_logo(lv_obj_t *canvas) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_vector_dsc_t *dsc = lv_vector_dsc_create(&layer);
    if (dsc == NULL) {
        lv_canvas_finish_layer(canvas, &layer);
        return false;
    }

    lv_vector_path_t *path = create_logo_path();
    if (path == NULL) {
        lv_vector_dsc_delete(dsc);
        lv_canvas_finish_layer(canvas, &layer);
        return false;
    }

    lv_vector_dsc_set_stroke_color(dsc, ui_theme_amber_bright());
    lv_vector_dsc_set_stroke_opa(dsc, LV_OPA_COVER);
    lv_vector_dsc_set_stroke_width(dsc, 8.0f);
    lv_vector_dsc_set_stroke_cap(dsc, LV_VECTOR_STROKE_CAP_BUTT);
    lv_vector_dsc_set_stroke_join(dsc, LV_VECTOR_STROKE_JOIN_MITER);
    lv_vector_dsc_add_path(dsc, path);
    lv_draw_vector(dsc);

    lv_vector_path_delete(path);
    lv_vector_dsc_delete(dsc);
    lv_canvas_finish_layer(canvas, &layer);
    return true;
}

static lv_vector_path_t *create_logo_path(void) {
    lv_vector_path_t *path = lv_vector_path_create(LV_VECTOR_PATH_QUALITY_MEDIUM);
    if (path == NULL) return NULL;
    const float bw = 78.0f, bh = 51.0f, cut = 15.0f;
    add_segment(path, 42, 69, 198, 69); add_segment(path, 198, 69, 213, 84);
    add_segment(path, 213, 84, 213, 156); add_segment(path, 213, 156, 198, 171);
    add_segment(path, 198, 171, 42, 171); add_segment(path, 42, 171, 27, 156);
    add_segment(path, 27, 156, 27, 84); add_segment(path, 27, 84, 42, 69);

    add_segment(path, 72, 145, 72, 95); add_segment(path, 72, 95, 95, 120);
    add_segment(path, 95, 120, 120, 95); add_segment(path, 120, 95, 120, 145);
    add_segment(path, 168, 95, 139, 95); add_segment(path, 139, 95, 139, 120);
    add_segment(path, 139, 120, 168, 120); add_segment(path, 168, 120, 168, 145);
    add_segment(path, 168, 145, 125, 145);
    (void)bw; (void)bh; (void)cut;
    return path;
}

boot_screen_t *boot_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    boot_screen_t *screen = calloc(1, sizeof(*screen));
    if (screen == NULL) return NULL;
    screen->root = lv_obj_create(parent);
    if (screen->root == NULL) { free(screen); return NULL; }
    lv_obj_remove_style_all(screen->root);
    lv_obj_set_size(screen->root, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(screen->root, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(screen->root, LV_OPA_COVER, 0);

    LV_DRAW_BUF_INIT_STATIC(draw_buf);
    screen->canvas = lv_canvas_create(screen->root);
    if (screen->canvas == NULL) {
        boot_screen_destroy(screen);
        return NULL;
    }
    lv_canvas_set_draw_buf(screen->canvas, &draw_buf);
    lv_obj_set_size(screen->canvas, BOOT_LOGO_SIZE, BOOT_LOGO_SIZE);
    lv_canvas_fill_bg(screen->canvas, ui_theme_amber_bg(), LV_OPA_COVER);
    lv_obj_update_layout(screen->canvas);
    if (!render_logo(screen->canvas)) {
        boot_screen_destroy(screen);
        return NULL;
    }
    lv_obj_center(screen->canvas);
    lv_obj_set_y(screen->canvas, -20);

    screen->label = lv_label_create(screen->root);
    if (screen->label == NULL) { boot_screen_destroy(screen); return NULL; }
    lv_label_set_text(screen->label, "SYSTEM STARTING");
    lv_obj_set_style_text_color(screen->label, ui_theme_amber_dim(), 0);
    lv_obj_set_style_text_font(screen->label, ui_font_or(ui_font_m, LV_FONT_DEFAULT), 0);
    lv_obj_center(screen->label);
    lv_obj_set_y(screen->label, 58);
    return screen;
}

void boot_screen_destroy(boot_screen_t *screen) {
    if (screen == NULL) return;
    if (screen->root != NULL) lv_obj_delete(screen->root);
    free(screen);
}
