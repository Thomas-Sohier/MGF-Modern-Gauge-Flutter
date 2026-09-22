#include "ui/screens/clock_screen.h"

#include "ui/fonts/ui_fonts.h"
#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Toutes les coordonnées restent dans le repère commun 320 px de l'écran
// ambre. Le disque réel fait 480 px de diamètre (échelle 1.5).
#define CLOCK_CX       (UI_REFERENCE_SIZE * 0.5f)
#define CLOCK_CY       CLOCK_CX
#define CLOCK_RADIUS   157.0f
#define LABEL_RADIUS   (CLOCK_RADIUS * 0.61f)
#define TICK_RADIUS    (CLOCK_RADIUS * 0.87f)
#define CARDINAL_COUNT 4

static const char *const k_cardinal_text[CARDINAL_COUNT] = {"12", "3", "6", "9"};
static const float k_cardinal_angle[CARDINAL_COUNT] = {-90.0f, 0.0f, 90.0f, 180.0f};

struct clock_screen_s {
    lv_obj_t *root;
    lv_obj_t *face;
    lv_obj_t *hands;
    lv_obj_t *labels[CARDINAL_COUNT];
    int hour;
    int minute;
};

static ui_layout_t layout_of(const lv_area_t *area) {
    ui_layout_t layout = ui_layout_fit(lv_area_get_width(area),
                                       lv_area_get_height(area));
    layout.ox += area->x1;
    layout.oy += area->y1;
    return layout;
}

static void draw_line(lv_layer_t *layer, float x1, float y1, float x2, float y2,
                      float width, lv_color_t color) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = color;
    d.opa = LV_OPA_COVER;
    d.width = LV_MAX(1, (int32_t)lroundf(width));
    d.round_start = 0;
    d.round_end = 0;
    d.p1.x = (int32_t)lroundf(x1);
    d.p1.y = (int32_t)lroundf(y1);
    d.p2.x = (int32_t)lroundf(x2);
    d.p2.y = (int32_t)lroundf(y2);
    lv_draw_line(layer, &d);
}

static void draw_circle(lv_layer_t *layer, float cx, float cy, float radius,
                        lv_color_t color) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    d.bg_opa = LV_OPA_COVER;
    d.radius = (int32_t)lroundf(radius);

    const int32_t r = (int32_t)lroundf(radius);
    const lv_area_t area = {
        (int32_t)lroundf(cx) - r,
        (int32_t)lroundf(cy) - r,
        (int32_t)lroundf(cx) + r,
        (int32_t)lroundf(cy) + r,
    };
    lv_draw_rect(layer, &d, &area);
}

static void draw_arc(lv_layer_t *layer, float cx, float cy, float radius,
                     float width, lv_color_t color) {
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center.x = (int32_t)lroundf(cx);
    d.center.y = (int32_t)lroundf(cy);
    d.radius = (int32_t)lroundf(radius);
    d.width = (int32_t)lroundf(width);
    d.start_angle = 0;
    d.end_angle = 360;
    d.color = color;
    d.opa = LV_OPA_COVER;
    d.rounded = 0;
    lv_draw_arc(layer, &d);
}

static void draw_face(lv_layer_t *layer, const ui_layout_t *layout) {
    const float cx = ui_layout_x(layout, CLOCK_CX);
    const float cy = ui_layout_y(layout, CLOCK_CY);
    const float scale = layout->scale;
    const float radius = CLOCK_RADIUS * scale;
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();

    // Un seul anneau garde la marge avec le masque circulaire et évite une
    // double bordure qui rivaliserait avec les repères.
    draw_arc(layer, cx, cy, radius - 1.0f * scale, 1.3f * scale,
             separator);

    for (int i = 0; i < 60; i++) {
        const float angle = (-90.0f + (float)i * 6.0f) * (float)M_PI / 180.0f;
        const float sine = sinf(angle);
        const float cosine = cosf(angle);

        if (i % 5 == 0) {
            const bool cardinal = (i % 15) == 0;
            const float inner = (cardinal ? 0.75f : 0.80f) * radius;
            const float outer = (cardinal ? 0.94f : 0.92f) * radius;
            draw_line(layer, cx + cosine * inner, cy + sine * inner,
                      cx + cosine * outer, cy + sine * outer,
                      (cardinal ? 2.4f : 1.7f) * scale,
                      cardinal ? bright : separator);
        } else {
            draw_circle(layer, cx + cosine * TICK_RADIUS * scale,
                        cy + sine * TICK_RADIUS * scale,
                        0.9f * scale, dim);
        }
    }
}

static void draw_hand(lv_layer_t *layer, float cx, float cy, float angle,
                      float length, float width, float offset, lv_color_t color) {
    const float radians = angle * (float)M_PI / 180.0f;
    const float x = cosf(radians);
    const float y = sinf(radians);
    draw_line(layer, cx + offset, cy + offset,
              cx + x * length + offset, cy + y * length + offset,
              width, color);
}

static void draw_hands(lv_layer_t *layer, const ui_layout_t *layout,
                       const clock_screen_t *screen) {
    const float cx = ui_layout_x(layout, CLOCK_CX);
    const float cy = ui_layout_y(layout, CLOCK_CY);
    const float scale = layout->scale;
    const float radius = CLOCK_RADIUS * scale;
    const float hour_angle = ((float)(screen->hour % 12) +
                              (float)screen->minute / 60.0f) * 30.0f - 90.0f;
    const float minute_angle = (float)screen->minute * 6.0f - 90.0f;
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();
    const lv_color_t bg = ui_theme_amber_bg();

    // Ombres ambre décalées : une bordure étroite détache les aiguilles sans
    // ajouter de contour clair ni d'effet coûteux. L'heure reste plus massive.
    const float hour_width = radius * 0.11f;
    const float minute_width = radius * 0.105f;
    draw_hand(layer, cx, cy, hour_angle, radius * 0.55f,
              hour_width + 1.6f * scale, 1.0f * scale, dim);
    draw_hand(layer, cx, cy, minute_angle, radius * 0.80f,
              minute_width + 1.6f * scale, 1.0f * scale, dim);

    draw_hand(layer, cx, cy, hour_angle, radius * 0.55f,
              hour_width, 0.0f, bright);
    draw_hand(layer, cx, cy, minute_angle, radius * 0.80f,
              minute_width, 0.0f, bright);

    // Pivot en trois couches : disque séparateur, centre sombre et crêtes
    // radiales inspirées du widget Flutter, toutes en teintes ambre.
    const float pivot = radius * 0.12f;
    draw_circle(layer, cx, cy, pivot, separator);
    draw_circle(layer, cx, cy, pivot * 0.78f, bg);
    for (int i = 0; i < 12; i++) {
        const float angle = (float)i * 30.0f * (float)M_PI / 180.0f;
        const float sine = sinf(angle);
        const float cosine = cosf(angle);
        draw_line(layer,
                  cx + cosine * pivot * 0.78f,
                  cy + sine * pivot * 0.78f,
                  cx + cosine * pivot * 0.96f,
                  cy + sine * pivot * 0.96f,
                  0.9f * scale, dim);
    }
}

static void face_draw_cb(lv_event_t *event) {
    lv_obj_t *object = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    if (layer == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(object, &area);
    const ui_layout_t layout = layout_of(&area);
    draw_face(layer, &layout);
}

static void hands_draw_cb(lv_event_t *event) {
    lv_obj_t *object = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    clock_screen_t *screen = lv_obj_get_user_data(object);
    if (layer == NULL || screen == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(object, &area);
    const ui_layout_t layout = layout_of(&area);
    draw_hands(layer, &layout, screen);
}

static bool read_local_time(int *hour, int *minute) {
#ifdef MGF_SIMULATOR
    *hour = 10;
    *minute = 10;
    return true;
#else
    const time_t now = time(NULL);
    const struct tm *local = now != (time_t)-1 ? localtime(&now) : NULL;
    if (local != NULL && local->tm_hour >= 0 && local->tm_hour < 24 &&
        local->tm_min >= 0 && local->tm_min < 60) {
        *hour = local->tm_hour;
        *minute = local->tm_min;
        return true;
    }

    // L'horloge système peut ne pas être synchronisée au démarrage. Une valeur
    // fixe garantit un cadran cohérent jusqu'à la prochaine lecture valide.
    *hour = 12;
    *minute = 0;
    return false;
#endif
}

clock_screen_t *clock_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    clock_screen_t *screen = lv_malloc(sizeof(*screen));
    if (screen == NULL) return NULL;
    lv_memzero(screen, sizeof(*screen));

    screen->root = amber_ui_root_create(parent);
    if (screen->root == NULL) goto fail;

    screen->face = amber_ui_canvas_create(screen->root, NULL, face_draw_cb);
    if (screen->face == NULL) goto fail;

    screen->hands = amber_ui_canvas_create(screen->root, screen, hands_draw_cb);
    if (screen->hands == NULL) goto fail;

    const lv_font_t *font = amber_ui_font_hero();
    for (int i = 0; i < CARDINAL_COUNT; i++) {
        screen->labels[i] = amber_ui_label_create(
            screen->root, font, ui_theme_amber_bright(), k_cardinal_text[i], 0.0f);
        if (screen->labels[i] == NULL) goto fail;

        const float angle = k_cardinal_angle[i] * (float)M_PI / 180.0f;
        amber_ui_place_centered(
            screen->labels[i], screen->root,
            CLOCK_CX + cosf(angle) * LABEL_RADIUS,
            CLOCK_CY + sinf(angle) * LABEL_RADIUS, 0.0f, 0.0f);
    }

    read_local_time(&screen->hour, &screen->minute);
    return screen;

fail:
    clock_screen_destroy(screen);
    return NULL;
}

void clock_screen_update(clock_screen_t *screen, const ecu_data_t *data) {
    (void)data;
    if (screen == NULL || screen->hands == NULL) return;

    int hour;
    int minute;
    read_local_time(&hour, &minute);
    if (hour == screen->hour && minute == screen->minute) return;

    screen->hour = hour;
    screen->minute = minute;
    lv_obj_invalidate(screen->hands);
}

void clock_screen_destroy(clock_screen_t *screen) {
    if (screen == NULL) return;
    if (screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
