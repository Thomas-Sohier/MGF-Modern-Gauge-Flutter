#include "ui/screens/settings_screen.h"

#include "domain/display_brightness.h"
#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>

// Terminal CRT ambre des années 80 (Esper / Spinner) sur deux pages de
// réglages : 01.ECRAN (luminosité, phosphore) et 02.SYST (page au démarrage,
// Bluetooth). Aucun cercle ni pilule : tous les cadres sont des octogones
// chanfreinés à 45° tracés à la main, sans jauge ni ornement parasite.
// Les onglets et le glissement horizontal changent de page ; OK (ou un
// appui hors des contrôles) referme la surimpression.
#define CX           160.0f
#define TABS_Y       74.0f
#define TAB_LEFT_X   104.0f
#define TAB_RIGHT_X  216.0f
#define TAB_W        102.0f
#define TAB_H        22.0f
#define TAB_CHAMFER  5.0f
#define TAB_TARGET_W 102.0f
#define TAB_TARGET_H 36.0f

#define CAPTION_1_Y 104.0f
#define ROW_1_Y     134.0f
#define CAPTION_2_Y 184.0f
#define ROW_2_Y     214.0f
#define TELEMETRY_Y 248.0f
#define OK_Y        278.0f

#define SIDE_LEFT    56.0f
#define SIDE_RIGHT   264.0f
#define SIDE_BOX_W   34.0f
#define SIDE_BOX_H   30.0f
#define SIDE_CHAMFER 6.0f
#define TARGET_SIZE  48.0f

#define BAR_X0  84.0f
#define BAR_X1  236.0f
#define BAR_H   22.0f
#define BAR_GAP 2.0f
#define LEVEL_X 272.0f

#define CARTOUCHE_W        196.0f
#define CARTOUCHE_H        34.0f
#define CARTOUCHE_CHAMFER  8.0f
#define CARTOUCHE_TARGET_H 44.0f

#define OK_W        88.0f
#define OK_H        28.0f
#define OK_CHAMFER  7.0f
#define OK_TARGET_W 96.0f
#define OK_TARGET_H 40.0f

#define PAGES 2U
// LVGL émet encore CLICKED après un glissement : on ignore les appuis qui
// suivent de près un changement de page (même garde que le navigateur).
#define GESTURE_CLICK_SUPPRESSION_MS 300U

struct settings_screen_s {
    settings_screen_actions_t actions;
    lv_obj_t *overlay;
    lv_obj_t *root;
    lv_obj_t *canvas;
    lv_obj_t *title;
    lv_obj_t *tab_text[PAGES];
    lv_obj_t *tab_target[PAGES];
    // Page 0
    lv_obj_t *level_caption;
    lv_obj_t *level_text;
    lv_obj_t *colors_caption;
    lv_obj_t *colors_text;
    // Page 1
    lv_obj_t *startup_caption;
    lv_obj_t *page_name;
    lv_obj_t *ble_caption;
    lv_obj_t *ble_text;
    // Bandeau télémétrique (résumé de l'autre page)
    lv_obj_t *telemetry;
    lv_obj_t *ok_text;
    lv_obj_t *minus;
    lv_obj_t *plus;
    lv_obj_t *previous;
    lv_obj_t *next;
    lv_obj_t *bluetooth;
    lv_obj_t *colors;
    lv_obj_t *ok;
    uint8_t level;
    uint8_t page_index;
    app_settings_page_t startup_page;
    settings_ble_state_t ble_state;
    uint32_t ble_remaining_s;
    uint32_t last_gesture_ms;
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

// ── Primitives chanfreinées ─────────────────────────────────────────────────
typedef struct {
    float x0;
    float y0;
    float x1;
    float y1;
} chamfer_box_t;

static chamfer_box_t chamfer_box(float cx, float cy, float w, float h) {
    const chamfer_box_t box = {cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f,
                               cy + h * 0.5f};
    return box;
}

static void fill_rect(lv_layer_t *layer, int32_t x0, int32_t y0, int32_t x1,
                      int32_t y1, lv_color_t color) {
    if (x1 <= x0 || y1 <= y0) return;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius = 0;
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    const lv_area_t area = {x0, y0, x1 - 1, y1 - 1};
    lv_draw_rect(layer, &dsc, &area);
}

// Remplissage plein d'un octogone : bande centrale + chanfreins en escalier
// de pixels physiques (LVGL ne sait dessiner que des rectangles).
static void chamfer_fill(lv_layer_t *layer, const ui_layout_t *layout,
                         const chamfer_box_t *box, float chamfer,
                         lv_color_t color) {
    const int32_t px0 = (int32_t)lroundf(ui_layout_x(layout, box->x0));
    const int32_t px1 = (int32_t)lroundf(ui_layout_x(layout, box->x1));
    const int32_t py0 = (int32_t)lroundf(ui_layout_y(layout, box->y0));
    const int32_t py1 = (int32_t)lroundf(ui_layout_y(layout, box->y1));
    const int32_t c = LV_MAX(1, (int32_t)lroundf(chamfer * layout->scale));
    if (px1 - px0 <= 3 * c || py1 - py0 <= 2 * c) {
        fill_rect(layer, px0, py0, px1, py1, color);
        return;
    }
    fill_rect(layer, px0, py0 + c, px1, py1 - c, color);
    for (int32_t i = 0; i < c; i++) {
        const int32_t inset = c - i;
        fill_rect(layer, px0 + inset, py0 + i, px1 - inset, py0 + i + 1, color);
        fill_rect(layer, px0 + inset, py1 - i - 1, px1 - inset, py1 - i, color);
    }
}

static void chamfer_outline(lv_layer_t *layer, const ui_layout_t *layout,
                            const chamfer_box_t *box, float chamfer,
                            float width, lv_color_t color) {
    const float pts[9][2] = {
        {box->x0 + chamfer, box->y0}, {box->x1 - chamfer, box->y0},
        {box->x1, box->y0 + chamfer}, {box->x1, box->y1 - chamfer},
        {box->x1 - chamfer, box->y1}, {box->x0 + chamfer, box->y1},
        {box->x0, box->y1 - chamfer}, {box->x0, box->y0 + chamfer},
        {box->x0 + chamfer, box->y0},
    };
    for (int i = 0; i < 8; i++) {
        amber_draw_line(layer, layout, pts[i][0], pts[i][1], pts[i + 1][0],
                        pts[i + 1][1], width, color, false);
    }
}

static void draw_cartouche(lv_layer_t *layer, const ui_layout_t *layout,
                           const chamfer_box_t *box, float chamfer,
                           lv_color_t color, bool filled) {
    if (filled) chamfer_fill(layer, layout, box, chamfer, color);
    chamfer_outline(layer, layout, box, chamfer, 1.4f, color);
}

static void draw_chevron(lv_layer_t *layer, const ui_layout_t *layout, float x,
                         float y, bool right, lv_color_t color) {
    const float d = right ? 1.0f : -1.0f;
    amber_draw_line(layer, layout, x - d * 3.5f, y - 8.0f, x + d * 4.5f, y,
                    2.4f, color, true);
    amber_draw_line(layer, layout, x + d * 4.5f, y, x - d * 3.5f, y + 8.0f,
                    2.4f, color, true);
}

// Bouton latéral − / + : cadre chanfreiné et signe net.
static void draw_sign_box(lv_layer_t *layer, const ui_layout_t *layout, float x,
                          float y, bool enabled, bool plus) {
    const lv_color_t color =
        enabled ? ui_theme_amber_bright() : ui_theme_amber_dim();
    const chamfer_box_t box = chamfer_box(x, y, SIDE_BOX_W, SIDE_BOX_H);
    chamfer_outline(layer, layout, &box, SIDE_CHAMFER, 1.5f, color);
    amber_draw_line(layer, layout, x - 7.0f, y, x + 7.0f, y, 2.6f, color, true);
    if (plus) {
        amber_draw_line(layer, layout, x, y - 7.0f, x, y + 7.0f, 2.6f, color,
                        true);
    }
}

// Bouton de pagination ‹ › : même cadre, chevron à la place du signe.
static void draw_nav_box(lv_layer_t *layer, const ui_layout_t *layout, float x,
                         float y, bool right) {
    const lv_color_t on = ui_theme_amber_bright();
    const chamfer_box_t box = chamfer_box(x, y, SIDE_BOX_W, SIDE_BOX_H);
    chamfer_outline(layer, layout, &box, SIDE_CHAMFER, 1.5f, on);
    draw_chevron(layer, layout, x, y, right, on);
}

// ── Tracés télémétriques ────────────────────────────────────────────────────
static void draw_tabs(lv_layer_t *layer, const ui_layout_t *layout,
                      const settings_screen_t *scr) {
    const float xs[PAGES] = {TAB_LEFT_X, TAB_RIGHT_X};
    for (unsigned i = 0; i < PAGES; i++) {
        const bool active = scr->page_index == (uint8_t)i;
        lv_color_t line = ui_theme_amber_separator();
        if (!active && i == 1U && scr->ble_state == SETTINGS_BLE_UNAVAILABLE) {
            line = ui_theme_amber_dim();
        }
        const chamfer_box_t box = chamfer_box(xs[i], TABS_Y, TAB_W, TAB_H);
        draw_cartouche(layer, layout, &box, TAB_CHAMFER,
                       active ? ui_theme_amber_bright() : line, active);
    }
}

static void draw_brightness(lv_layer_t *layer, const ui_layout_t *layout,
                            const settings_screen_t *scr) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const float base = ROW_1_Y + BAR_H * 0.5f;
    const float pitch = (BAR_X1 - BAR_X0) / (float)DISPLAY_BRIGHTNESS_LEVELS;
    const float width = pitch - BAR_GAP;

    draw_sign_box(layer, layout, SIDE_LEFT, ROW_1_Y, scr->level > 1U, false);
    draw_sign_box(layer, layout, SIDE_RIGHT, ROW_1_Y,
                  scr->level < DISPLAY_BRIGHTNESS_LEVELS, true);

    // Bargraphe CRT : un segment vertical par cran matériel du AW9364.
    for (unsigned i = 0; i < DISPLAY_BRIGHTNESS_LEVELS; i++) {
        const float x = BAR_X0 + pitch * ((float)i + 0.5f);
        amber_draw_line(layer, layout, x, base - BAR_H, x, base, width,
                        i < scr->level ? bright : dim, false);
    }
}

// Palette : cartouche en contour avec la palette normale, plein quand la
// palette inversée est active (même grammaire que le BLE).
static void draw_colors(lv_layer_t *layer, const ui_layout_t *layout,
                        const settings_screen_t *scr) {
    const bool inverted = scr->color_mode == APP_SETTINGS_COLORS_INVERTED;
    const chamfer_box_t box =
        chamfer_box(CX, ROW_2_Y, CARTOUCHE_W, CARTOUCHE_H);
    draw_cartouche(layer, layout, &box, CARTOUCHE_CHAMFER,
                   inverted ? ui_theme_amber_bright()
                            : ui_theme_amber_separator(),
                   inverted);
}

static void draw_bluetooth(lv_layer_t *layer, const ui_layout_t *layout,
                           const settings_screen_t *scr) {
    const chamfer_box_t box =
        chamfer_box(CX, ROW_2_Y, CARTOUCHE_W, CARTOUCHE_H);
    switch (scr->ble_state) {
    case SETTINGS_BLE_OPEN:
    case SETTINGS_BLE_CONNECTED:
        draw_cartouche(layer, layout, &box, CARTOUCHE_CHAMFER,
                       ui_theme_amber_bright(), true);
        break;
    case SETTINGS_BLE_CLOSED:
        draw_cartouche(layer, layout, &box, CARTOUCHE_CHAMFER,
                       ui_theme_amber_separator(), false);
        break;
    case SETTINGS_BLE_UNAVAILABLE:
    default:
        draw_cartouche(layer, layout, &box, CARTOUCHE_CHAMFER,
                       ui_theme_amber_dim(), false);
        break;
    }
}

static void draw_ok(lv_layer_t *layer, const ui_layout_t *layout) {
    const chamfer_box_t box = chamfer_box(CX, OK_Y, OK_W, OK_H);
    draw_cartouche(layer, layout, &box, OK_CHAMFER, ui_theme_amber_bright(),
                   true);
}

static void canvas_draw_cb(lv_event_t *event) {
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    settings_screen_t *scr = lv_obj_get_user_data(canvas);
    if (layer == NULL || scr == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);

    amber_kit_draw_title_rules(layer, &layout, scr->title, AMBER_KIT_TITLE_Y);
    draw_tabs(layer, &layout, scr);

    if (scr->page_index == 0U) {
        draw_brightness(layer, &layout, scr);
        draw_colors(layer, &layout, scr);
    } else {
        draw_nav_box(layer, &layout, SIDE_LEFT, ROW_1_Y, false);
        draw_nav_box(layer, &layout, SIDE_RIGHT, ROW_1_Y, true);
        draw_bluetooth(layer, &layout, scr);
    }
    draw_ok(layer, &layout);
}

// ── Textes ──────────────────────────────────────────────────────────────────
static void set_hidden(lv_obj_t *object, bool hidden) {
    if (object == NULL) return;
    if (hidden)
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
}

static void set_label(lv_obj_t *label, const char *text, lv_color_t color,
                      float x, float y, amber_align_t align) {
    if (label == NULL) return;
    lv_obj_set_style_text_color(label, color, 0);
    if (amber_kit_set_text(label, text)) amber_kit_place(label, x, y, align);
}

// Résumé court de la fenêtre d'appairage, partagé par l'onglet et le bandeau.
static void ble_pairing_text(char *buffer, size_t size,
                             const settings_screen_t *scr) {
    if (scr->ble_remaining_s == 0U) {
        // Sans échéance (image de banc ouverte au boot) : pas de décompte.
        snprintf(buffer, size, "APPAIRAGE");
        return;
    }
    snprintf(buffer, size, "APPAIRAGE %u:%02u",
             (unsigned)(scr->ble_remaining_s / 60U),
             (unsigned)(scr->ble_remaining_s % 60U));
}

static const char *tab_ble_label(const settings_screen_t *scr) {
    switch (scr->ble_state) {
    case SETTINGS_BLE_OPEN:
        return "02.PAIR";
    case SETTINGS_BLE_CONNECTED:
        return "02.LIE";
    case SETTINGS_BLE_CLOSED:
        return "02.SYST";
    case SETTINGS_BLE_UNAVAILABLE:
    default:
        return "02.OFF";
    }
}

// Une seule passe : textes, couleurs de palette et visibilités de page. Elle
// est rejouée à chaque changement d'état, y compris lors d'une bascule de
// palette, pour que la création directe et la bascule à chaud coïncident au
// pixel près.
static void refresh_state(settings_screen_t *scr) {
    if (scr == NULL) return;
    const lv_color_t bg = ui_theme_amber_bg();
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t sep = ui_theme_amber_separator();
    const lv_color_t dim = ui_theme_amber_dim();
    const bool page0 = scr->page_index == 0U;
    const bool inverted = scr->color_mode == APP_SETTINGS_COLORS_INVERTED;

    // Onglets de pagination : texte sombre sur l'onglet actif (plein), teinte
    // d'état sinon, pour que l'état BLE reste lisible depuis la page 0.
    lv_color_t ble_tone = bright;
    switch (scr->ble_state) {
    case SETTINGS_BLE_OPEN:
    case SETTINGS_BLE_CONNECTED:
        ble_tone = bright;
        break;
    case SETTINGS_BLE_CLOSED:
        ble_tone = sep;
        break;
    case SETTINGS_BLE_UNAVAILABLE:
    default:
        ble_tone = dim;
        break;
    }
    set_label(scr->tab_text[0], "01.ECRAN", page0 ? bg : sep, TAB_LEFT_X,
              TABS_Y, AMBER_ALIGN_CENTER);
    set_label(scr->tab_text[1], tab_ble_label(scr), page0 ? ble_tone : bg,
              TAB_RIGHT_X, TABS_Y, AMBER_ALIGN_CENTER);

    // Page 0 : luminosité et phosphore.
    char level_text[8];
    snprintf(level_text, sizeof(level_text), "%u/%u", (unsigned)scr->level,
             (unsigned)DISPLAY_BRIGHTNESS_LEVELS);
    set_label(scr->level_caption, "LUMINOSITE", sep, BAR_X0, CAPTION_1_Y,
              AMBER_ALIGN_LEFT);
    set_label(scr->level_text, level_text, bright, LEVEL_X, CAPTION_1_Y,
              AMBER_ALIGN_RIGHT);
    set_label(scr->colors_caption, "PHOSPHORE CRT", sep, CX, CAPTION_2_Y,
              AMBER_ALIGN_CENTER);
    set_label(scr->colors_text, inverted ? "INVERSEES" : "AMBRE NORMAL",
              inverted ? bg : bright, CX, ROW_2_Y, AMBER_ALIGN_CENTER);

    // Page 1 : page au démarrage et liaison Bluetooth.
    set_label(scr->startup_caption, "PAGE AU DEMARRAGE", sep, CX, CAPTION_1_Y,
              AMBER_ALIGN_CENTER);
    set_label(scr->page_name, page_label(scr->startup_page), bright, CX,
              ROW_1_Y, AMBER_ALIGN_CENTER);
    set_label(scr->ble_caption, "BLUETOOTH", sep, CX, CAPTION_2_Y,
              AMBER_ALIGN_CENTER);
    char ble[24];
    lv_color_t ble_color = bright;
    switch (scr->ble_state) {
    case SETTINGS_BLE_OPEN:
        ble_pairing_text(ble, sizeof(ble), scr);
        ble_color = bg;
        break;
    case SETTINGS_BLE_CONNECTED:
        snprintf(ble, sizeof(ble), "TELEPHONE LIE");
        ble_color = bg;
        break;
    case SETTINGS_BLE_CLOSED:
        snprintf(ble, sizeof(ble), "APPAIRER");
        ble_color = sep;
        break;
    case SETTINGS_BLE_UNAVAILABLE:
    default:
        snprintf(ble, sizeof(ble), "INDISPONIBLE");
        ble_color = dim;
        break;
    }
    set_label(scr->ble_text, ble, ble_color, CX, ROW_2_Y, AMBER_ALIGN_CENTER);

    // Bandeau télémétrique : l'état BLE reste lisible depuis la page 0, la
    // luminosité et la palette depuis la page 1.
    char telemetry[32];
    lv_color_t telemetry_color =
        scr->ble_state == SETTINGS_BLE_UNAVAILABLE ? dim : sep;
    if (page0) {
        switch (scr->ble_state) {
        case SETTINGS_BLE_OPEN: {
            char pairing[16];
            ble_pairing_text(pairing, sizeof(pairing), scr);
            snprintf(telemetry, sizeof(telemetry), "BLE: %s", pairing);
            break;
        }
        case SETTINGS_BLE_CONNECTED:
            snprintf(telemetry, sizeof(telemetry), "BLE: TELEPHONE LIE");
            break;
        case SETTINGS_BLE_CLOSED:
            snprintf(telemetry, sizeof(telemetry), "BLE: VEILLE");
            break;
        case SETTINGS_BLE_UNAVAILABLE:
        default:
            snprintf(telemetry, sizeof(telemetry), "BLE: INDISPONIBLE");
            break;
        }
    } else {
        snprintf(telemetry, sizeof(telemetry), "LUM %s %s", level_text,
                 inverted ? "INVERSE" : "AMBRE");
        telemetry_color = sep;
    }
    set_label(scr->telemetry, telemetry, telemetry_color, CX, TELEMETRY_Y,
              AMBER_ALIGN_CENTER);

    // Visibilité : les labels et les cibles de l'autre page sont masqués (une
    // cible masquée ne capte plus les appuis).
    lv_obj_t *const page_0[] = {scr->level_caption,  scr->level_text,
                                scr->colors_caption, scr->colors_text,
                                scr->minus,          scr->plus,
                                scr->colors};
    lv_obj_t *const page_1[] = {
        scr->startup_caption, scr->page_name, scr->ble_caption, scr->ble_text,
        scr->previous,        scr->next,      scr->bluetooth};
    for (size_t i = 0; i < sizeof(page_0) / sizeof(page_0[0]); i++) {
        set_hidden(page_0[i], !page0);
    }
    for (size_t i = 0; i < sizeof(page_1) / sizeof(page_1[0]); i++) {
        set_hidden(page_1[i], page0);
    }
}

// ── Interaction ─────────────────────────────────────────────────────────────
static void set_page(settings_screen_t *scr, uint8_t page) {
    if (page >= PAGES || page == scr->page_index) return;
    scr->page_index = page;
    refresh_state(scr);
    lv_obj_invalidate(scr->canvas);
}

static void set_level(settings_screen_t *scr, int level) {
    if (level < 1) level = 1; // jamais d'écran noir sans retour possible
    if (level > (int)DISPLAY_BRIGHTNESS_LEVELS) {
        level = (int)DISPLAY_BRIGHTNESS_LEVELS;
    }
    if ((uint8_t)level == scr->level) return;
    scr->level = (uint8_t)level;
    refresh_state(scr);
    lv_obj_invalidate(scr->canvas);
    if (scr->actions.brightness_changed != NULL) {
        scr->actions.brightness_changed(
            scr->actions.context,
            display_brightness_percent_for_level(scr->level));
    }
}

static void set_color_mode(settings_screen_t *scr,
                           app_settings_color_mode_t mode) {
    if (mode < 0 || mode >= APP_SETTINGS_COLORS_COUNT) return;
    if (mode == scr->color_mode) return;
    scr->color_mode = mode;
    refresh_state(scr);
    lv_obj_invalidate(scr->canvas);
}

static void step_startup_page(settings_screen_t *scr, int delta) {
    // Cycle : DERNIERE VUE, puis chaque page dans l'ordre du swipe.
    const int count = (int)APP_SETTINGS_STARTUP_LAST_PAGE + 1;
    const int next = ((int)scr->startup_page + delta + count) % count;
    scr->startup_page = (app_settings_page_t)next;
    refresh_state(scr);
    if (scr->actions.startup_page_changed != NULL) {
        scr->actions.startup_page_changed(scr->actions.context,
                                          scr->startup_page);
    }
}

// Un glissement vient de changer de page : l'appui qui suit (CLICKED émis au
// relâcher par LVGL) ne doit rien déclencher d'autre.
static bool click_suppressed(const settings_screen_t *scr) {
    return (uint32_t)(lv_tick_get() - scr->last_gesture_ms) <
           GESTURE_CLICK_SUPPRESSION_MS;
}

static void button_event_cb(lv_event_t *event) {
    lv_obj_t *target = lv_event_get_target(event);
    settings_screen_t *scr = lv_event_get_user_data(event);
    if (scr == NULL || click_suppressed(scr)) return;

    if (target == scr->tab_target[0])
        set_page(scr, 0);
    else if (target == scr->tab_target[1])
        set_page(scr, 1);
    else if (target == scr->minus)
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

// Glissement horizontal sur la surimpression : bascule de page. Les cibles
// gardent GESTURE_BUBBLE pour que le geste parte de n'importe où sur le
// disque et s'arrête ici (la surimpression n'a pas le drapeau).
static void overlay_gesture_cb(lv_event_t *event) {
    settings_screen_t *scr = lv_event_get_user_data(event);
    lv_indev_t *indev = lv_event_get_indev(event);
    if (scr == NULL || indev == NULL) return;
    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction != LV_DIR_LEFT && direction != LV_DIR_RIGHT) return;
    scr->last_gesture_ms = lv_tick_get();
    set_page(scr, scr->page_index == 0U ? 1U : 0U);
}

// Appui hors des contrôles : ferme la surimpression, comme OK. Le press
// d'ouverture (glissement ou maintien) vise la page sous la surimpression,
// LVGL n'émet donc pas de CLICKED ici au relâcher.
static void overlay_event_cb(lv_event_t *event) {
    settings_screen_t *scr = lv_event_get_user_data(event);
    if (scr == NULL || click_suppressed(scr)) return;
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
    // GESTURE_BUBBLE conservé : un glissement démarré sur une cible remonte
    // jusqu'à la surimpression et change de page.
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(button, button_event_cb, LV_EVENT_CLICKED, scr);
    return button;
}

static lv_obj_t *create_caption(settings_screen_t *scr, const char *text,
                                float x, float y, amber_align_t align) {
    lv_obj_t *label = amber_kit_caption(scr->root, text);
    if (label != NULL) amber_kit_place(label, x, y, align);
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
    scr->last_gesture_ms = lv_tick_get() - GESTURE_CLICK_SUPPRESSION_MS;

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
    lv_obj_add_event_cb(scr->overlay, overlay_gesture_cb, LV_EVENT_GESTURE,
                        scr);
    lv_obj_update_layout(scr->overlay);

    scr->root = amber_ui_root_create(scr->overlay);
    if (scr->root == NULL) goto fail;
    scr->canvas = amber_ui_canvas_create(scr->root, scr, canvas_draw_cb);
    if (scr->canvas == NULL) goto fail;

    scr->title = amber_kit_caption(scr->root, "REGLAGES");
    if (scr->title == NULL) goto fail;
    amber_kit_place(scr->title, CX, AMBER_KIT_TITLE_Y, AMBER_ALIGN_CENTER);

    scr->level_caption = create_caption(scr, "LUMINOSITE", BAR_X0, CAPTION_1_Y,
                                        AMBER_ALIGN_LEFT);
    scr->colors_caption = create_caption(scr, "PHOSPHORE CRT", CX, CAPTION_2_Y,
                                         AMBER_ALIGN_CENTER);
    scr->startup_caption = create_caption(scr, "PAGE AU DEMARRAGE", CX,
                                          CAPTION_1_Y, AMBER_ALIGN_CENTER);
    scr->ble_caption =
        create_caption(scr, "BLUETOOTH", CX, CAPTION_2_Y, AMBER_ALIGN_CENTER);
    if (scr->level_caption == NULL || scr->colors_caption == NULL ||
        scr->startup_caption == NULL || scr->ble_caption == NULL) {
        goto fail;
    }

    scr->level_text = amber_kit_label(scr->root, amber_kit_font_caption(),
                                      ui_theme_amber_bright(), "");
    scr->colors_text = amber_kit_label(scr->root, amber_kit_font_caption(),
                                       ui_theme_amber_bright(), "");
    scr->page_name = amber_kit_label(scr->root, amber_kit_font_caption(),
                                     ui_theme_amber_bright(), "");
    scr->ble_text = amber_kit_label(scr->root, amber_kit_font_caption(),
                                    ui_theme_amber_bright(), "");
    scr->telemetry = amber_kit_label(scr->root, amber_kit_font_caption(),
                                     ui_theme_amber_separator(), "");
    scr->ok_text = amber_kit_label(scr->root, amber_kit_font_label(),
                                   ui_theme_amber_bg(), "OK");
    if (scr->level_text == NULL || scr->colors_text == NULL ||
        scr->page_name == NULL || scr->ble_text == NULL ||
        scr->telemetry == NULL || scr->ok_text == NULL) {
        goto fail;
    }
    amber_kit_place(scr->ok_text, CX, OK_Y, AMBER_ALIGN_CENTER);
    for (unsigned i = 0; i < PAGES; i++) {
        scr->tab_text[i] = amber_kit_label(scr->root, amber_kit_font_caption(),
                                           ui_theme_amber_separator(), "");
        if (scr->tab_text[i] == NULL) goto fail;
    }

    scr->minus =
        create_target(scr, SIDE_LEFT, ROW_1_Y, TARGET_SIZE, TARGET_SIZE);
    scr->plus =
        create_target(scr, SIDE_RIGHT, ROW_1_Y, TARGET_SIZE, TARGET_SIZE);
    scr->previous =
        create_target(scr, SIDE_LEFT, ROW_1_Y, TARGET_SIZE, TARGET_SIZE);
    scr->next =
        create_target(scr, SIDE_RIGHT, ROW_1_Y, TARGET_SIZE, TARGET_SIZE);
    scr->colors =
        create_target(scr, CX, ROW_2_Y, CARTOUCHE_W, CARTOUCHE_TARGET_H);
    scr->bluetooth =
        create_target(scr, CX, ROW_2_Y, CARTOUCHE_W, CARTOUCHE_TARGET_H);
    scr->tab_target[0] =
        create_target(scr, TAB_LEFT_X, TABS_Y, TAB_TARGET_W, TAB_TARGET_H);
    scr->tab_target[1] =
        create_target(scr, TAB_RIGHT_X, TABS_Y, TAB_TARGET_W, TAB_TARGET_H);
    scr->ok = create_target(scr, CX, OK_Y, OK_TARGET_W, OK_TARGET_H);
    if (scr->minus == NULL || scr->plus == NULL || scr->previous == NULL ||
        scr->next == NULL || scr->colors == NULL || scr->bluetooth == NULL ||
        scr->tab_target[0] == NULL || scr->tab_target[1] == NULL ||
        scr->ok == NULL) {
        goto fail;
    }

    refresh_state(scr);
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
    const bool level_changed = level != scr->level;
    scr->level = level;
    scr->startup_page = settings->startup_page;
    const app_settings_color_mode_t mode = settings->color_mode;
    const bool mode_valid = mode >= 0 && mode < APP_SETTINGS_COLORS_COUNT;
    const bool mode_changed = mode_valid && mode != scr->color_mode;
    if (mode_valid) scr->color_mode = mode;
    // Rejoué même à valeur égale : après une bascule de palette, les rôles de
    // ui_theme_amber_*() ont changé et tous les labels doivent les relire.
    refresh_state(scr);
    if (level_changed || mode_changed) lv_obj_invalidate(scr->canvas);
}

void settings_screen_show(settings_screen_t *scr,
                          const app_settings_t *settings) {
    if (scr == NULL) return;
    scr->page_index = 0;
    scr->last_gesture_ms = lv_tick_get() - GESTURE_CLICK_SUPPRESSION_MS;
    settings_screen_set_settings(scr, settings);
    lv_obj_invalidate(scr->canvas);
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
    const bool state_changed = state != scr->ble_state;
    scr->ble_state = state;
    scr->ble_remaining_s = remaining_s;
    refresh_state(scr);
    if (state_changed) lv_obj_invalidate(scr->canvas);
}

void settings_screen_destroy(settings_screen_t *scr) {
    if (scr == NULL) return;
    if (scr->overlay != NULL) lv_obj_delete(scr->overlay);
    lv_free(scr);
}
