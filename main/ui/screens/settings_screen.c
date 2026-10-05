#include "ui/screens/settings_screen.h"

#include "domain/display_brightness.h"
#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>

// Quatre réglages empilés dans le repère 320, chacun avec sa légende :
// luminosité (−, 16 crans du AW9364, +), page de démarrage (‹ nom ›),
// fenêtre Bluetooth et palette (pilules d'état). Le bouton OK referme la
// surimpression.
#define CX            160.0f
#define BRIGHT_CAP_Y  74.0f
#define BRIGHT_ROW_Y  95.0f
#define RULE_1_Y      116.0f
#define STARTUP_CAP_Y 131.0f
#define STARTUP_ROW_Y 152.0f
#define RULE_2_Y      173.0f
#define BLE_CAP_Y     188.0f
#define BLE_ROW_Y     210.0f
#define COLORS_CAP_Y  238.0f
#define COLORS_ROW_Y  260.0f
#define OK_Y          294.0f

#define SIDE_LEFT   64.0f
#define SIDE_RIGHT  256.0f
#define SIDE_RADIUS 16.0f
#define OK_RADIUS   17.0f
#define STEPS_LEFT  92.0f
#define STEPS_RIGHT 228.0f
#define STEP_GAP    2.0f
#define STEP_HEIGHT 16.0f
#define PILL_WIDTH  172.0f
#define PILL_HEIGHT 28.0f
#define TARGET_SIZE 44.0f
// Cibles tactiles des pilules et de OK : pas de recouvrement vertical entre
// la pilule du bas (246..274) et OK (276..312).
#define PILL_TARGET_HEIGHT 32.0f
#define OK_TARGET_WIDTH    64.0f
#define OK_TARGET_HEIGHT   36.0f

struct settings_screen_s {
    settings_screen_actions_t actions;
    lv_obj_t *overlay;
    lv_obj_t *root;
    lv_obj_t *canvas;
    lv_obj_t *title;
    lv_obj_t *page_name;
    lv_obj_t *ble_text;
    lv_obj_t *colors_text;
    lv_obj_t *ok_text;
    lv_obj_t *minus;
    lv_obj_t *plus;
    lv_obj_t *previous;
    lv_obj_t *next;
    lv_obj_t *bluetooth;
    lv_obj_t *colors;
    lv_obj_t *ok;
    uint8_t level;
    app_settings_page_t startup_page;
    settings_ble_state_t ble_state;
    uint32_t ble_remaining_s;
    app_settings_color_mode_t color_mode;
};

static const char *page_label(app_settings_page_t page) {
    static const char *const labels[APP_SETTINGS_PAGE_COUNT] = {
        [APP_SETTINGS_PAGE_CLOCK] = "HORLOGE",
        [APP_SETTINGS_PAGE_MUSIC] = "MUSIQUE",
        [APP_SETTINGS_PAGE_NAVIGATION] = "NAVIGATION",
        [APP_SETTINGS_PAGE_RPM] = "COMPTE-TOURS",
        [APP_SETTINGS_PAGE_FAULTS] = "DIAGNOSTIC",
        [APP_SETTINGS_PAGE_TEMPERATURES] = "TEMPERATURES",
        [APP_SETTINGS_PAGE_INJECTION] = "INJECTION",
        [APP_SETTINGS_PAGE_LAMBDA] = "RICHESSE",
        [APP_SETTINGS_PAGE_IGNITION] = "ALLUMAGE",
        [APP_SETTINGS_PAGE_IDLE] = "RALENTI",
        [APP_SETTINGS_PAGE_ADMISSION] = "ADMISSION",
    };
    if (page >= 0 && page < APP_SETTINGS_PAGE_COUNT) return labels[page];
    return "DERNIERE VUE";
}

// ── Dessin ──────────────────────────────────────────────────────────────────
static void draw_rounded_rect(lv_layer_t *layer, const ui_layout_t *layout,
                              float cx, float cy, float w, float h,
                              lv_color_t color, bool filled) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = LV_RADIUS_CIRCLE;
    if (filled) {
        dsc.bg_color = color;
        dsc.bg_opa = LV_OPA_COVER;
    } else {
        dsc.bg_opa = LV_OPA_TRANSP;
        dsc.border_color = color;
        dsc.border_opa = LV_OPA_COVER;
        dsc.border_width = LV_MAX(1, (int32_t)lroundf(1.4f * layout->scale));
    }
    const lv_area_t area = {
        (int32_t)lroundf(ui_layout_x(layout, cx - w * 0.5f)),
        (int32_t)lroundf(ui_layout_y(layout, cy - h * 0.5f)),
        (int32_t)lroundf(ui_layout_x(layout, cx + w * 0.5f)),
        (int32_t)lroundf(ui_layout_y(layout, cy + h * 0.5f)),
    };
    lv_draw_rect(layer, &dsc, &area);
}

static void draw_chevron(lv_layer_t *layer, const ui_layout_t *layout, float x,
                         float y, bool right, lv_color_t color) {
    const float d = right ? 1.0f : -1.0f;
    amber_draw_line(layer, layout, x - d * 3.0f, y - 7.0f, x + d * 4.0f, y,
                    2.4f, color, true);
    amber_draw_line(layer, layout, x + d * 4.0f, y, x - d * 3.0f, y + 7.0f,
                    2.4f, color, true);
}

static void draw_side_button(lv_layer_t *layer, const ui_layout_t *layout,
                             float x, float y, bool enabled) {
    amber_draw_circle(layer, layout, x, y, SIDE_RADIUS, 1.4f,
                      enabled ? ui_theme_amber_separator()
                              : ui_theme_amber_dim());
}

static void draw_brightness(lv_layer_t *layer, const ui_layout_t *layout,
                            const settings_screen_t *scr) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const bool can_lower = scr->level > 1U;
    const bool can_raise = scr->level < DISPLAY_BRIGHTNESS_LEVELS;

    draw_side_button(layer, layout, SIDE_LEFT, BRIGHT_ROW_Y, can_lower);
    amber_draw_line(layer, layout, SIDE_LEFT - 6.0f, BRIGHT_ROW_Y,
                    SIDE_LEFT + 6.0f, BRIGHT_ROW_Y, 2.4f,
                    can_lower ? bright : dim, true);
    draw_side_button(layer, layout, SIDE_RIGHT, BRIGHT_ROW_Y, can_raise);
    amber_draw_line(layer, layout, SIDE_RIGHT - 6.0f, BRIGHT_ROW_Y,
                    SIDE_RIGHT + 6.0f, BRIGHT_ROW_Y, 2.4f,
                    can_raise ? bright : dim, true);
    amber_draw_line(layer, layout, SIDE_RIGHT, BRIGHT_ROW_Y - 6.0f, SIDE_RIGHT,
                    BRIGHT_ROW_Y + 6.0f, 2.4f, can_raise ? bright : dim, true);

    // Un cran par niveau matériel, hauteur croissante : lecture immédiate de
    // la position dans la plage.
    const float pitch = (STEPS_RIGHT - STEPS_LEFT) / DISPLAY_BRIGHTNESS_LEVELS;
    const float width = pitch - STEP_GAP;
    for (unsigned i = 0; i < DISPLAY_BRIGHTNESS_LEVELS; i++) {
        const float x = STEPS_LEFT + pitch * (float)i + pitch * 0.5f;
        const float h =
            STEP_HEIGHT *
            (0.45f + 0.55f * (float)i / (DISPLAY_BRIGHTNESS_LEVELS - 1U));
        const float bottom = BRIGHT_ROW_Y + STEP_HEIGHT * 0.5f;
        amber_draw_line(layer, layout, x, bottom - h, x, bottom, width,
                        i < scr->level ? bright : dim, false);
    }
}

static void draw_bluetooth(lv_layer_t *layer, const ui_layout_t *layout,
                           const settings_screen_t *scr) {
    switch (scr->ble_state) {
    case SETTINGS_BLE_OPEN:
    case SETTINGS_BLE_CONNECTED:
        draw_rounded_rect(layer, layout, CX, BLE_ROW_Y, PILL_WIDTH, PILL_HEIGHT,
                          ui_theme_amber_bright(), true);
        break;
    case SETTINGS_BLE_CLOSED:
        draw_rounded_rect(layer, layout, CX, BLE_ROW_Y, PILL_WIDTH, PILL_HEIGHT,
                          ui_theme_amber_separator(), false);
        break;
    case SETTINGS_BLE_UNAVAILABLE:
    default:
        draw_rounded_rect(layer, layout, CX, BLE_ROW_Y, PILL_WIDTH, PILL_HEIGHT,
                          ui_theme_amber_dim(), false);
        break;
    }
}

// Pilule de palette : contour en couleurs normales, pleine quand la palette
// inversée est active (même grammaire que la pilule Bluetooth).
static void draw_colors(lv_layer_t *layer, const ui_layout_t *layout,
                        const settings_screen_t *scr) {
    const bool inverted = scr->color_mode == APP_SETTINGS_COLORS_INVERTED;
    draw_rounded_rect(layer, layout, CX, COLORS_ROW_Y, PILL_WIDTH, PILL_HEIGHT,
                      inverted ? ui_theme_amber_bright()
                               : ui_theme_amber_separator(),
                      inverted);
}

static void canvas_draw_cb(lv_event_t *event) {
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    settings_screen_t *scr = lv_obj_get_user_data(canvas);
    if (layer == NULL || scr == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);
    const lv_color_t sep = ui_theme_amber_separator();

    amber_kit_draw_title_rules(layer, &layout, scr->title, AMBER_KIT_TITLE_Y);
    amber_draw_line(layer, &layout, 70.0f, RULE_1_Y, 250.0f, RULE_1_Y, 1.0f,
                    sep, false);
    amber_draw_line(layer, &layout, 70.0f, RULE_2_Y, 250.0f, RULE_2_Y, 1.0f,
                    sep, false);

    draw_brightness(layer, &layout, scr);

    draw_side_button(layer, &layout, SIDE_LEFT, STARTUP_ROW_Y, true);
    draw_chevron(layer, &layout, SIDE_LEFT, STARTUP_ROW_Y, false,
                 ui_theme_amber_bright());
    draw_side_button(layer, &layout, SIDE_RIGHT, STARTUP_ROW_Y, true);
    draw_chevron(layer, &layout, SIDE_RIGHT, STARTUP_ROW_Y, true,
                 ui_theme_amber_bright());

    draw_bluetooth(layer, &layout, scr);
    draw_colors(layer, &layout, scr);
    amber_draw_dot(layer, &layout, CX, OK_Y, OK_RADIUS,
                   ui_theme_amber_bright());
}

// ── Textes ──────────────────────────────────────────────────────────────────
static void refresh_page_name(settings_screen_t *scr) {
    if (amber_kit_set_text(scr->page_name, page_label(scr->startup_page))) {
        amber_kit_place(scr->page_name, CX, STARTUP_ROW_Y, AMBER_ALIGN_CENTER);
    }
}

static void refresh_bluetooth_text(settings_screen_t *scr) {
    char text[24];
    lv_color_t color = ui_theme_amber_bright();
    switch (scr->ble_state) {
    case SETTINGS_BLE_OPEN:
        // Sans échéance (image de banc ouverte au boot) : pas de décompte.
        if (scr->ble_remaining_s == 0U) {
            snprintf(text, sizeof(text), "APPAIRAGE");
        } else {
            snprintf(text, sizeof(text), "APPAIRAGE %u:%02u",
                     (unsigned)(scr->ble_remaining_s / 60U),
                     (unsigned)(scr->ble_remaining_s % 60U));
        }
        color = ui_theme_amber_bg();
        break;
    case SETTINGS_BLE_CONNECTED:
        snprintf(text, sizeof(text), "TELEPHONE LIE");
        color = ui_theme_amber_bg();
        break;
    case SETTINGS_BLE_CLOSED:
        snprintf(text, sizeof(text), "APPAIRER");
        break;
    case SETTINGS_BLE_UNAVAILABLE:
    default:
        snprintf(text, sizeof(text), "INDISPONIBLE");
        color = ui_theme_amber_dim();
        break;
    }
    lv_obj_set_style_text_color(scr->ble_text, color, 0);
    if (amber_kit_set_text(scr->ble_text, text)) {
        amber_kit_place(scr->ble_text, CX, BLE_ROW_Y, AMBER_ALIGN_CENTER);
    }
}

static void refresh_colors_text(settings_screen_t *scr) {
    const bool inverted = scr->color_mode == APP_SETTINGS_COLORS_INVERTED;
    lv_obj_set_style_text_color(
        scr->colors_text,
        inverted ? ui_theme_amber_bg() : ui_theme_amber_bright(), 0);
    if (amber_kit_set_text(scr->colors_text,
                           inverted ? "INVERSEES" : "NORMALES")) {
        amber_kit_place(scr->colors_text, CX, COLORS_ROW_Y, AMBER_ALIGN_CENTER);
    }
}

static void set_color_mode(settings_screen_t *scr,
                           app_settings_color_mode_t mode) {
    if (mode < 0 || mode >= APP_SETTINGS_COLORS_COUNT) return;
    if (mode == scr->color_mode) return;
    scr->color_mode = mode;
    lv_obj_invalidate(scr->canvas);
    refresh_colors_text(scr);
}

// ── Interaction ─────────────────────────────────────────────────────────────
static void set_level(settings_screen_t *scr, int level) {
    if (level < 1) level = 1; // jamais d'écran noir sans retour possible
    if (level > (int)DISPLAY_BRIGHTNESS_LEVELS) {
        level = (int)DISPLAY_BRIGHTNESS_LEVELS;
    }
    if ((uint8_t)level == scr->level) return;
    scr->level = (uint8_t)level;
    lv_obj_invalidate(scr->canvas);
    if (scr->actions.brightness_changed != NULL) {
        scr->actions.brightness_changed(
            scr->actions.context,
            display_brightness_percent_for_level(scr->level));
    }
}

static void step_startup_page(settings_screen_t *scr, int delta) {
    // Cycle : DERNIERE VUE, puis chaque page dans l'ordre du swipe.
    const int count = (int)APP_SETTINGS_STARTUP_LAST_PAGE + 1;
    const int next = ((int)scr->startup_page + delta + count) % count;
    scr->startup_page = (app_settings_page_t)next;
    refresh_page_name(scr);
    if (scr->actions.startup_page_changed != NULL) {
        scr->actions.startup_page_changed(scr->actions.context,
                                          scr->startup_page);
    }
}

static void button_event_cb(lv_event_t *event) {
    lv_obj_t *target = lv_event_get_target(event);
    settings_screen_t *scr = lv_event_get_user_data(event);
    if (scr == NULL) return;

    if (target == scr->minus)
        set_level(scr, (int)scr->level - 1);
    else if (target == scr->plus)
        set_level(scr, (int)scr->level + 1);
    else if (target == scr->previous)
        step_startup_page(scr, -1);
    else if (target == scr->next)
        step_startup_page(scr, 1);
    else if (target == scr->bluetooth) {
        // APPAIRER ouvre la fenêtre, un second appui la referme ; téléphone
        // déjà lié ou BLE absent : rien à faire.
        if (scr->ble_state != SETTINGS_BLE_CLOSED &&
            scr->ble_state != SETTINGS_BLE_OPEN) {
            return;
        }
        if (scr->actions.bluetooth_toggled != NULL) {
            scr->actions.bluetooth_toggled(
                scr->actions.context, scr->ble_state == SETTINGS_BLE_CLOSED);
        }
    } else if (target == scr->colors) {
        const app_settings_color_mode_t next =
            scr->color_mode == APP_SETTINGS_COLORS_INVERTED
                ? APP_SETTINGS_COLORS_NORMAL
                : APP_SETTINGS_COLORS_INVERTED;
        set_color_mode(scr, next);
        if (scr->actions.color_mode_changed != NULL) {
            scr->actions.color_mode_changed(scr->actions.context, next);
        }
    } else if (target == scr->ok) {
        settings_screen_hide(scr);
        if (scr->actions.closed != NULL) {
            scr->actions.closed(scr->actions.context);
        }
    }
}

// Appui hors des contrôles : ferme la surimpression, comme OK. Le press
// d'ouverture (glissement ou maintien) vise la page sous la surimpression,
// LVGL n'émet donc pas de CLICKED ici au relâcher.
static void overlay_event_cb(lv_event_t *event) {
    settings_screen_t *scr = lv_event_get_user_data(event);
    if (scr == NULL) return;
    settings_screen_hide(scr);
    if (scr->actions.closed != NULL) scr->actions.closed(scr->actions.context);
}

static lv_obj_t *create_target(settings_screen_t *scr, float x, float y,
                               float w, float h) {
    const ui_layout_t layout = ui_layout_fit(lv_obj_get_width(scr->root),
                                             lv_obj_get_height(scr->root));
    const int32_t pw = (int32_t)lroundf(w * layout.scale);
    const int32_t ph = (int32_t)lroundf(h * layout.scale);
    lv_obj_t *button = lv_obj_create(scr->root);
    if (button == NULL) return NULL;
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, pw, ph);
    lv_obj_set_pos(button, (int32_t)lroundf(ui_layout_x(&layout, x)) - pw / 2,
                   (int32_t)lroundf(ui_layout_y(&layout, y)) - ph / 2);
    lv_obj_clear_flag(button,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_CLICKED, scr);
    return button;
}

static lv_obj_t *create_caption(settings_screen_t *scr, const char *text,
                                float y) {
    lv_obj_t *label = amber_kit_caption(scr->root, text);
    if (label != NULL) {
        amber_kit_place(label, CX, y, AMBER_ALIGN_CENTER);
    }
    return label;
}

settings_screen_t *
settings_screen_create(lv_obj_t *parent,
                       const settings_screen_actions_t *actions) {
    if (parent == NULL || actions == NULL) return NULL;
    settings_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));
    scr->actions = *actions;
    scr->level = DISPLAY_BRIGHTNESS_LEVELS;
    scr->startup_page = APP_SETTINGS_STARTUP_LAST_PAGE;
    scr->ble_state = SETTINGS_BLE_UNAVAILABLE;

    // Surimpression opaque et cliquable : elle masque le dashboard (LVGL ne
    // redessine pas ce qui est entièrement couvert) et absorbe les appuis et
    // gestes pour qu'ils n'atteignent pas le navigateur.
    scr->overlay = lv_obj_create(parent);
    if (scr->overlay == NULL) goto fail;
    lv_obj_remove_style_all(scr->overlay);
    lv_obj_set_size(scr->overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_center(scr->overlay);
    lv_obj_set_style_bg_color(scr->overlay, ui_theme_amber_bg(), 0);
    lv_obj_set_style_bg_opa(scr->overlay, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr->overlay,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_flag(scr->overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(scr->overlay, overlay_event_cb, LV_EVENT_CLICKED, scr);
    lv_obj_update_layout(scr->overlay);

    scr->root = amber_ui_root_create(scr->overlay);
    if (scr->root == NULL) goto fail;
    scr->canvas = amber_ui_canvas_create(scr->root, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    scr->title = amber_kit_caption(scr->root, "REGLAGES");
    if (scr->title == NULL) goto fail;
    amber_kit_place(scr->title, CX, AMBER_KIT_TITLE_Y, AMBER_ALIGN_CENTER);

    if (create_caption(scr, "LUMINOSITE", BRIGHT_CAP_Y) == NULL ||
        create_caption(scr, "PAGE AU DEMARRAGE", STARTUP_CAP_Y) == NULL ||
        create_caption(scr, "BLUETOOTH", BLE_CAP_Y) == NULL ||
        create_caption(scr, "COULEURS", COLORS_CAP_Y) == NULL) {
        goto fail;
    }

    scr->page_name = amber_kit_label(scr->root, amber_kit_font_caption(),
                                     ui_theme_amber_bright(), "");
    scr->ble_text = amber_kit_label(scr->root, amber_kit_font_caption(),
                                    ui_theme_amber_bright(), "");
    scr->colors_text = amber_kit_label(scr->root, amber_kit_font_caption(),
                                       ui_theme_amber_bright(), "");
    scr->ok_text = amber_kit_label(scr->root, amber_kit_font_label(),
                                   ui_theme_amber_bg(), "OK");
    if (scr->page_name == NULL || scr->ble_text == NULL ||
        scr->colors_text == NULL || scr->ok_text == NULL) {
        goto fail;
    }
    amber_kit_place(scr->ok_text, CX, OK_Y, AMBER_ALIGN_CENTER);
    refresh_page_name(scr);
    refresh_bluetooth_text(scr);
    refresh_colors_text(scr);

    scr->minus =
        create_target(scr, SIDE_LEFT, BRIGHT_ROW_Y, TARGET_SIZE, TARGET_SIZE);
    scr->plus =
        create_target(scr, SIDE_RIGHT, BRIGHT_ROW_Y, TARGET_SIZE, TARGET_SIZE);
    scr->previous =
        create_target(scr, SIDE_LEFT, STARTUP_ROW_Y, TARGET_SIZE, TARGET_SIZE);
    scr->next =
        create_target(scr, SIDE_RIGHT, STARTUP_ROW_Y, TARGET_SIZE, TARGET_SIZE);
    scr->bluetooth =
        create_target(scr, CX, BLE_ROW_Y, PILL_WIDTH, PILL_TARGET_HEIGHT);
    scr->colors =
        create_target(scr, CX, COLORS_ROW_Y, PILL_WIDTH, PILL_TARGET_HEIGHT);
    scr->ok = create_target(scr, CX, OK_Y, OK_TARGET_WIDTH, OK_TARGET_HEIGHT);
    if (scr->minus == NULL || scr->plus == NULL || scr->previous == NULL ||
        scr->next == NULL || scr->bluetooth == NULL || scr->colors == NULL ||
        scr->ok == NULL) {
        goto fail;
    }

    lv_obj_add_flag(scr->overlay, LV_OBJ_FLAG_HIDDEN);
    return scr;

fail:
    settings_screen_destroy(scr);
    return NULL;
}

void settings_screen_set_settings(settings_screen_t *scr,
                                  const app_settings_t *settings) {
    if (scr == NULL || settings == NULL) return;
    uint8_t level = display_brightness_level(settings->brightness_percent);
    if (level < 1U) level = 1U;
    if (level != scr->level) {
        scr->level = level;
        lv_obj_invalidate(scr->canvas);
    }
    scr->startup_page = settings->startup_page;
    refresh_page_name(scr);
    set_color_mode(scr, settings->color_mode);
}

void settings_screen_show(settings_screen_t *scr,
                          const app_settings_t *settings) {
    if (scr == NULL) return;
    settings_screen_set_settings(scr, settings);
    lv_obj_move_foreground(scr->overlay);
    lv_obj_remove_flag(scr->overlay, LV_OBJ_FLAG_HIDDEN);
}

void settings_screen_hide(settings_screen_t *scr) {
    if (scr == NULL || scr->overlay == NULL) return;
    lv_obj_add_flag(scr->overlay, LV_OBJ_FLAG_HIDDEN);
}

bool settings_screen_is_visible(const settings_screen_t *scr) {
    return scr != NULL && scr->overlay != NULL &&
           !lv_obj_has_flag(scr->overlay, LV_OBJ_FLAG_HIDDEN);
}

void settings_screen_set_bluetooth(settings_screen_t *scr,
                                   settings_ble_state_t state,
                                   uint32_t remaining_s) {
    if (scr == NULL) return;
    if (state != SETTINGS_BLE_OPEN) remaining_s = 0;
    if (state == scr->ble_state && remaining_s == scr->ble_remaining_s) {
        return;
    }
    if (state != scr->ble_state) lv_obj_invalidate(scr->canvas);
    scr->ble_state = state;
    scr->ble_remaining_s = remaining_s;
    refresh_bluetooth_text(scr);
}

void settings_screen_destroy(settings_screen_t *scr) {
    if (scr == NULL) return;
    if (scr->overlay != NULL) lv_obj_delete(scr->overlay);
    lv_free(scr);
}
