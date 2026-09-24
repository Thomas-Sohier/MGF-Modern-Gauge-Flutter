#include "ui/screens/navigation_screen.h"

#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"

#include <math.h>
#include <string.h>

// Guidage reçu de l'application compagnon (notification Google Maps) :
// pictogramme de manœuvre vectoriel, distance en valeur héros, consigne et
// voie sous un filet, heure d'arrivée dans la ligne d'état commune. La
// manœuvre est déduite du texte de la consigne : l'icône PNG envoyée par le
// téléphone n'est pas exploitée par le rendu ambre.
#define DISTANCE_Y       158.0f
#define INSTRUCTION_Y    199.0f
#define STREET_Y         225.0f
#define TEXT_WIDTH       236.0f
#define ROUTE_CORE_W       5.0f
#define ROUTE_ROAD_W      15.0f
#define ARROW_CORE_W       6.0f
#define ARROW_ROAD_W      17.0f
#define HEAD_LEN          15.0f

struct navigation_screen_s {
    amber_page_t page;
    lv_obj_t *instruction;
    lv_obj_t *street;
    companion_nav_t nav;
    bool linked;
};

typedef struct {
    float x;
    float y;
} point_t;

// Tracé d'une manœuvre « à droite » dans le repère 320 ; les variantes à
// gauche sont obtenues par symétrie autour de x = 160.
typedef struct {
    point_t points[9];
    unsigned count;
    bool ring; // rond-point : anneau autour de (160, 92)
    bool pin;  // arrivée : pastille en bout de trait
} route_shape_t;

static route_shape_t shape_of(companion_maneuver_t maneuver, bool *mirror) {
    *mirror = false;
    switch (maneuver) {
    case COMPANION_MANEUVER_LEFT:
        *mirror = true;
        // fall through
    case COMPANION_MANEUVER_RIGHT:
        return (route_shape_t){{{140, 124}, {140, 100}, {158, 82}, {196, 82}}, 4,
                               false, false};
    case COMPANION_MANEUVER_SLIGHT_LEFT:
        *mirror = true;
        // fall through
    case COMPANION_MANEUVER_SLIGHT_RIGHT:
        return (route_shape_t){{{150, 126}, {150, 104}, {184, 70}}, 3, false,
                               false};
    case COMPANION_MANEUVER_SHARP_LEFT:
        *mirror = true;
        // fall through
    case COMPANION_MANEUVER_SHARP_RIGHT:
        return (route_shape_t){{{142, 126}, {142, 74}, {186, 118}}, 3, false,
                               false};
    case COMPANION_MANEUVER_UTURN:
        // Demi-tour par la gauche (circulation à droite).
        return (route_shape_t){{{182, 126}, {182, 92}, {178, 79}, {170, 71},
                                {160, 68}, {150, 71}, {142, 79}, {138, 92},
                                {138, 122}}, 9, false, false};
    case COMPANION_MANEUVER_ROUNDABOUT:
        return (route_shape_t){{{160, 128}, {160, 112}}, 2, true, false};
    case COMPANION_MANEUVER_ARRIVE:
        return (route_shape_t){{{160, 126}, {160, 96}}, 2, false, true};
    case COMPANION_MANEUVER_STRAIGHT:
    case COMPANION_MANEUVER_UNKNOWN:
    default:
        return (route_shape_t){{{160, 126}, {160, 66}}, 2, false, false};
    }
}

static float mx(float x, bool mirror) {
    return mirror ? 2.0f * AMBER_KIT_CX - x : x;
}

// Pointe de flèche au bout du segment (a -> b).
static void draw_head(lv_layer_t *layer, const ui_layout_t *layout, point_t a,
                      point_t b, float width, lv_color_t color) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len <= 0.0f) return;
    const float ux = dx / len;
    const float uy = dy / len;
    // Deux branches à ±45° de la direction inverse.
    const float c = 0.70710678f;
    const float bx = -ux;
    const float by = -uy;
    const point_t arm1 = {b.x + HEAD_LEN * (bx * c - by * c),
                          b.y + HEAD_LEN * (bx * c + by * c)};
    const point_t arm2 = {b.x + HEAD_LEN * (bx * c + by * c),
                          b.y + HEAD_LEN * (-bx * c + by * c)};
    amber_draw_line(layer, layout, b.x, b.y, arm1.x, arm1.y, width, color, true);
    amber_draw_line(layer, layout, b.x, b.y, arm2.x, arm2.y, width, color, true);
}

static void draw_route(lv_layer_t *layer, const ui_layout_t *layout,
                       void *context) {
    const navigation_screen_t *scr = context;
    const lv_color_t sep = ui_theme_amber_separator();
    // Filet entre distance et consigne, dans l'esprit de la grille commune.
    amber_draw_line(layer, layout, 72.0f, 181.0f, 248.0f, 181.0f, 1.333f, sep,
                    false);
    if (scr == NULL || !scr->nav.active) {
        // Pas de guidage : repère discret à la place du pictogramme.
        amber_draw_circle(layer, layout, AMBER_KIT_CX, 96.0f, 22.0f, 1.4f,
                          ui_theme_amber_dim());
        amber_draw_dot(layer, layout, AMBER_KIT_CX, 96.0f, 4.0f,
                       ui_theme_amber_dim());
        return;
    }

    bool mirror;
    const route_shape_t shape = shape_of(scr->nav.maneuver, &mirror);
    point_t pts[9];
    for (unsigned i = 0; i < shape.count; i++) {
        pts[i] = (point_t){mx(shape.points[i].x, mirror), shape.points[i].y};
    }

    for (unsigned pass = 0; pass < 2; pass++) {
        const float width = pass == 0 ? ROUTE_ROAD_W : ROUTE_CORE_W;
        const float head_w = pass == 0 ? ARROW_ROAD_W : ARROW_CORE_W;
        const lv_color_t color = pass == 0 ? ui_theme_amber_dim()
                                           : ui_theme_amber_bright();
        for (unsigned i = 0; i + 1U < shape.count; i++) {
            amber_draw_line(layer, layout, pts[i].x, pts[i].y, pts[i + 1U].x,
                            pts[i + 1U].y, width, color, true);
        }
        if (shape.ring) {
            // Rond-point (sens giratoire, circulation à droite) : la chaussée
            // dessine l'anneau complet, le trait lumineux n'en suit que la
            // portion parcourue (entrée en bas, sortie en haut à droite).
            const float r = 20.0f;
            if (pass == 0) {
                amber_draw_circle(layer, layout, AMBER_KIT_CX, 92.0f,
                                  r + width * 0.5f, width, color);
            } else {
                amber_draw_arc_wrapped(layer, layout, AMBER_KIT_CX, 92.0f,
                                       r + width * 0.5f, width, 315.0f, 135.0f,
                                       color, true);
            }
            const point_t exit_a = {AMBER_KIT_CX + r * 0.7071f,
                                    92.0f - r * 0.7071f};
            const point_t exit_b = {194.0f, 58.0f};
            amber_draw_line(layer, layout, exit_a.x, exit_a.y, exit_b.x,
                            exit_b.y, width, color, true);
            draw_head(layer, layout, exit_a, exit_b, head_w, color);
        } else if (shape.pin) {
            const point_t end = pts[shape.count - 1U];
            amber_draw_dot(layer, layout, end.x, end.y - 12.0f,
                           pass == 0 ? 15.0f : 9.0f, color);
        } else if (shape.count >= 2U) {
            draw_head(layer, layout, pts[shape.count - 2U],
                      pts[shape.count - 1U], head_w, color);
        }
    }
}

static void refresh(navigation_screen_t *scr) {
    const companion_nav_t *nav = &scr->nav;
    if (!nav->active) {
        amber_page_set_hero(&scr->page, "--", false);
        amber_readout_set_unit(&scr->page.hero, "");
        if (amber_kit_set_text(scr->instruction,
                               scr->linked ? "AUCUN GUIDAGE" : "TELEPHONE")) {
            amber_kit_place(scr->instruction, AMBER_KIT_CX, INSTRUCTION_Y,
                            AMBER_ALIGN_CENTER);
        }
        lv_obj_set_style_text_color(scr->instruction,
                                    ui_theme_amber_separator(), 0);
        if (amber_kit_set_text(scr->street, scr->linked ? "" : "NON CONNECTE")) {
            amber_kit_place(scr->street, AMBER_KIT_CX, STREET_Y,
                            AMBER_ALIGN_CENTER);
        }
        amber_page_set_status(&scr->page, "", false);
        amber_page_invalidate(&scr->page);
        return;
    }

    // Distance numérique en héros ; un texte (« MAINTENANT ») reste lisible
    // en consigne plutôt que de déborder en 56 px.
    const bool numeric = nav->distance_value[0] >= '0' &&
                         nav->distance_value[0] <= '9';
    amber_page_set_hero(&scr->page, numeric ? nav->distance_value : "--",
                        numeric);
    amber_readout_set_unit(&scr->page.hero, numeric ? nav->distance_unit : "");

    const char *instruction = nav->instruction[0] != '\0'
        ? nav->instruction
        : (!numeric && nav->distance_value[0] != '\0') ? nav->distance_value
        : "SUIVRE L'ITINERAIRE";
    if (amber_kit_set_text(scr->instruction, instruction)) {
        amber_kit_place(scr->instruction, AMBER_KIT_CX, INSTRUCTION_Y,
                        AMBER_ALIGN_CENTER);
    }
    lv_obj_set_style_text_color(scr->instruction, ui_theme_amber_bright(), 0);
    if (amber_kit_set_text(scr->street, nav->street)) {
        amber_kit_place(scr->street, AMBER_KIT_CX, STREET_Y, AMBER_ALIGN_CENTER);
    }
    amber_page_set_status(&scr->page, nav->eta, true);
    amber_page_invalidate(&scr->page);
}

navigation_screen_t *navigation_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    navigation_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));

    const amber_page_spec_t spec = {
        .title = "NAVIGATION",
        .hero_unit = "",
        .rim = AMBER_RIM_NONE,
        .no_grid = true,
        .draw_extra = draw_route,
        .context = scr,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) goto fail;

    // La valeur héros descend sous le pictogramme de manœuvre.
    scr->page.hero.y = DISTANCE_Y;
    amber_readout_layout(&scr->page.hero);

    lv_obj_t *root = scr->page.root;
    const ui_layout_t layout = ui_layout_fit(lv_obj_get_width(root),
                                             lv_obj_get_height(root));
    scr->instruction = amber_kit_label(root, amber_kit_font_label(),
                                       ui_theme_amber_bright(), "");
    scr->street = amber_kit_caption(root, "");
    if (scr->instruction == NULL || scr->street == NULL) goto fail;
    const int32_t width = (int32_t)lroundf(TEXT_WIDTH * layout.scale);
    lv_obj_set_width(scr->instruction, width);
    lv_label_set_long_mode(scr->instruction, LV_LABEL_LONG_DOT);
    lv_obj_set_width(scr->street, width);
    lv_label_set_long_mode(scr->street, LV_LABEL_LONG_DOT);
    refresh(scr);
    return scr;

fail:
    navigation_screen_destroy(scr);
    return NULL;
}

void navigation_screen_set_route(navigation_screen_t *scr,
                                 const companion_nav_t *nav) {
    if (scr == NULL) return;
    if (nav != NULL) {
        scr->nav = *nav;
    } else {
        memset(&scr->nav, 0, sizeof(scr->nav));
    }
    refresh(scr);
}

void navigation_screen_set_link(navigation_screen_t *scr, bool linked) {
    if (scr == NULL || scr->linked == linked) return;
    scr->linked = linked;
    // Un guidage ne survit pas à la perte du téléphone.
    if (!linked) memset(&scr->nav, 0, sizeof(scr->nav));
    refresh(scr);
}

void navigation_screen_destroy(navigation_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    lv_free(scr);
}
