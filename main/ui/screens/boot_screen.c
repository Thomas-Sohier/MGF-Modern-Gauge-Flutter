#include "ui/screens/boot_screen.h"

#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"
#include "ui/widgets/amber_ui.h"

#define BOOT_CENTER 160.0f

struct boot_screen {
    lv_obj_t *root;
    lv_obj_t *canvas;
    lv_obj_t *label;
};

static void draw_segmented_frame(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();
    const lv_color_t bright = ui_theme_amber_bright();

    // Anneau fin, volontairement interrompu en bas : il respecte la marge du
    // panneau rond et laisse respirer le libellé de démarrage.
    amber_draw_arc_wrapped(layer, layout, BOOT_CENTER, BOOT_CENTER, 140.0f,
                           1.0f, 208.0f, 304.0f, dim, false);
    amber_draw_arc_wrapped(layer, layout, BOOT_CENTER, BOOT_CENTER, 140.0f,
                           1.0f, 152.0f, 42.0f, separator, false);
    amber_draw_arc_wrapped(layer, layout, BOOT_CENTER, BOOT_CENTER, 140.0f,
                           1.0f, 238.0f, 38.0f, bright, false);

    // Deux repères horizontaux ancrent le signe sans fermer une grille autour
    // du cadran.
    amber_draw_line(layer, layout, 48.0f, 136.0f, 78.0f, 136.0f, 1.0f,
                    separator, false);
    amber_draw_line(layer, layout, 242.0f, 136.0f, 272.0f, 136.0f, 1.0f,
                    separator, false);
    amber_draw_dot(layer, layout, 83.0f, 136.0f, 1.5f, dim);
    amber_draw_dot(layer, layout, 237.0f, 136.0f, 1.5f, dim);
}

static void draw_badge(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t separator = ui_theme_amber_separator();
    const float points[][2] = {
        {108.0f, 93.0f}, {212.0f, 93.0f}, {228.0f, 109.0f},
        {228.0f, 163.0f}, {212.0f, 179.0f}, {108.0f, 179.0f},
        {92.0f, 163.0f}, {92.0f, 109.0f},
    };

    for (unsigned i = 0; i < sizeof(points) / sizeof(points[0]); i++) {
        const unsigned next = (i + 1u) % (sizeof(points) / sizeof(points[0]));
        amber_draw_line(layer, layout, points[i][0], points[i][1],
                        points[next][0], points[next][1], 1.4f, separator,
                        false);
    }

    // Une traverse discrète donne au monogramme une assise automobile, sans
    // ajouter d'état ou d'information non disponible au démarrage.
    amber_draw_line(layer, layout, 105.0f, 163.0f, 215.0f, 163.0f, 0.8f,
                    separator, false);
}

static void draw_mgf_mark(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t bright = ui_theme_amber_bright();
    const float width = 3.0f;

    // M
    amber_draw_line(layer, layout, 107.0f, 151.0f, 107.0f, 119.0f, width,
                    bright, true);
    amber_draw_line(layer, layout, 107.0f, 119.0f, 120.0f, 134.0f, width,
                    bright, true);
    amber_draw_line(layer, layout, 120.0f, 134.0f, 133.0f, 119.0f, width,
                    bright, true);
    amber_draw_line(layer, layout, 133.0f, 119.0f, 133.0f, 151.0f, width,
                    bright, true);

    // G : l'ouverture et la traverse restent très lisibles à faible luminosité.
    amber_draw_arc_wrapped(layer, layout, 157.0f, 135.0f, 16.0f, width,
                           42.0f, 276.0f, bright, true);
    amber_draw_line(layer, layout, 157.0f, 135.0f, 174.0f, 135.0f, width,
                    bright, true);

    // F
    amber_draw_line(layer, layout, 184.0f, 151.0f, 184.0f, 119.0f, width,
                    bright, true);
    amber_draw_line(layer, layout, 184.0f, 119.0f, 211.0f, 119.0f, width,
                    bright, true);
    amber_draw_line(layer, layout, 184.0f, 135.0f, 204.0f, 135.0f, width,
                    bright, true);
}

static void boot_canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_layer_t *layer = lv_event_get_layer(event);
    lv_obj_t *canvas = lv_event_get_target(event);
    if (layer == NULL || canvas == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);
    draw_segmented_frame(layer, &layout);
    draw_badge(layer, &layout);
    draw_mgf_mark(layer, &layout);
}

boot_screen_t *boot_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    boot_screen_t *screen = lv_malloc(sizeof(*screen));
    if (screen == NULL) return NULL;
    lv_memzero(screen, sizeof(*screen));

    screen->root = amber_ui_root_create(parent);
    if (screen->root == NULL) goto fail;

    screen->canvas = amber_ui_canvas_create(screen->root, NULL,
                                            boot_canvas_draw_cb);
    if (screen->canvas == NULL) goto fail;

    screen->label = amber_kit_caption(screen->root, "DEMARRAGE");
    if (screen->label == NULL) goto fail;
    lv_obj_set_style_text_letter_space(screen->label, 3, 0);
    amber_kit_place(screen->label, BOOT_CENTER, 222.0f, AMBER_ALIGN_CENTER);

    return screen;

fail:
    boot_screen_destroy(screen);
    return NULL;
}

void boot_screen_destroy(boot_screen_t *screen) {
    if (screen == NULL) return;
    if (screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
