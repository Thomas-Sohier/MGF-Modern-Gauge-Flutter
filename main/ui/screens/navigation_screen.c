#include "ui/screens/navigation_screen.h"

#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"

#include <math.h>
#include <string.h>

// Guidage reçu de l'application compagnon (notification Google Maps) rendu
// comme un tracé vectoriel anguleux, épuré :
//   - aucune jauge ni couronne : le cadran reste nu ;
//   - la manœuvre est un vecteur de cap (rail éteint + veine vive), remplacé
//     par une mire octogonale sobre quand aucun guidage n'est actif ;
//   - la distance est la valeur héros, la consigne puis la voie suivent en
//     ambre vif : l'espacement suffit à la hiérarchie, sans filet, crochets
//     ni croix de visée.
// La manœuvre est déduite du texte de la consigne : l'icône PNG envoyée par
// le téléphone n'est pas exploitée par le rendu ambre.
#define DISTANCE_Y    156.0f
#define INSTRUCTION_Y 198.0f
#define STREET_Y      224.0f
#define TEXT_WIDTH    244.0f

// Centre de la zone de manœuvre et de la mire hors-ligne.
#define HUD_CY       96.0f
#define ROUTE_RAIL_W 11.0f // gabarit de couloir, bords francs
#define ROUTE_CORE_W 3.8f  // vecteur de cap lumineux
#define HEAD_LEN     14.0f
#define HEAD_W       3.8f
#define RING_R       17.0f
#define RING_CY      97.0f

// Mire hors-ligne centrée sur la zone de manœuvre.
#define RADAR_R 22.0f

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
    bool ring; // rond-point : anneau autour de (160, 97)
    bool pin;  // arrivée : cible d'acquisition en bout de trait
} route_shape_t;

static route_shape_t shape_of(companion_maneuver_t maneuver, bool *mirror) {
    *mirror = false;
    switch (maneuver) {
    case COMPANION_MANEUVER_LEFT:
        *mirror = true;
        // fall through
    case COMPANION_MANEUVER_RIGHT:
        return (route_shape_t){
            {{140, 124}, {140, 100}, {158, 82}, {196, 82}}, 4, false, false};
    case COMPANION_MANEUVER_SLIGHT_LEFT:
        *mirror = true;
        // fall through
    case COMPANION_MANEUVER_SLIGHT_RIGHT:
        return (route_shape_t){
            {{150, 126}, {150, 104}, {184, 70}}, 3, false, false};
    case COMPANION_MANEUVER_SHARP_LEFT:
        *mirror = true;
        // fall through
    case COMPANION_MANEUVER_SHARP_RIGHT:
        return (route_shape_t){
            {{142, 126}, {142, 74}, {186, 118}}, 3, false, false};
    case COMPANION_MANEUVER_UTURN:
        // Demi-tour par la gauche (circulation à droite).
        return (route_shape_t){{{182, 126},
                                {182, 92},
                                {178, 79},
                                {170, 71},
                                {160, 68},
                                {150, 71},
                                {142, 79},
                                {138, 92},
                                {138, 122}},
                               9,
                               false,
                               false};
    case COMPANION_MANEUVER_ROUNDABOUT:
        return (route_shape_t){{{160, 130}, {160, 114}}, 2, true, false};
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

// ── Primitives de tracé ─────────────────────────────────────────────────────
static void draw_octagon(lv_layer_t *layer, const ui_layout_t *layout, float cx,
                         float cy, float radius, float width,
                         lv_color_t color) {
    point_t pts[8];
    for (int i = 0; i < 8; i++) {
        // Sommets décalés de 22,5° : pans hauts/bas/latéraux à plat.
        const float angle = (22.5f + 45.0f * (float)i) * 0.0174532925f;
        pts[i] =
            (point_t){cx + radius * cosf(angle), cy + radius * sinf(angle)};
    }
    for (int i = 0; i < 8; i++) {
        const point_t a = pts[i];
        const point_t b = pts[(i + 1) % 8];
        amber_draw_line(layer, layout, a.x, a.y, b.x, b.y, width, color, false);
    }
}

static void draw_diamond(lv_layer_t *layer, const ui_layout_t *layout, float cx,
                         float cy, float radius, float width,
                         lv_color_t color) {
    amber_draw_line(layer, layout, cx - radius, cy, cx, cy - radius, width,
                    color, false);
    amber_draw_line(layer, layout, cx, cy - radius, cx + radius, cy, width,
                    color, false);
    amber_draw_line(layer, layout, cx + radius, cy, cx, cy + radius, width,
                    color, false);
    amber_draw_line(layer, layout, cx, cy + radius, cx - radius, cy, width,
                    color, false);
}

// Chevron ouvert vers l'arrière depuis la pointe (tx, ty), de direction (ux,
// uy) : ailettes à 45° ciselées, sans arrondi.
static void draw_chevron(lv_layer_t *layer, const ui_layout_t *layout, float tx,
                         float ty, float ux, float uy, float len, float width,
                         lv_color_t color) {
    const float c = 0.70710678f;
    const float bx = -ux;
    const float by = -uy;
    amber_draw_line(layer, layout, tx, ty, tx + len * (bx * c - by * c),
                    ty + len * (bx * c + by * c), width, color, false);
    amber_draw_line(layer, layout, tx, ty, tx + len * (bx * c + by * c),
                    ty + len * (-bx * c + by * c), width, color, false);
}

// Tête de flèche vectorielle + chevron d'écho en retrait sur la tige.
static void draw_head(lv_layer_t *layer, const ui_layout_t *layout, point_t a,
                      point_t b, lv_color_t color) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len <= 0.0f) return;
    const float ux = dx / len;
    const float uy = dy / len;
    draw_chevron(layer, layout, b.x, b.y, ux, uy, HEAD_LEN, HEAD_W, color);
    const float back = HEAD_LEN * 1.15f;
    draw_chevron(layer, layout, b.x - ux * back, b.y - uy * back, ux, uy,
                 HEAD_LEN * 0.62f, HEAD_W * 0.8f, color);
}

// Mire hors-ligne : simple octogone à pans coupés et point central.
static void draw_radar_mire(lv_layer_t *layer, const ui_layout_t *layout) {
    const lv_color_t dim = ui_theme_amber_dim();
    draw_octagon(layer, layout, AMBER_KIT_CX, HUD_CY, RADAR_R, 1.3f, dim);
    amber_draw_dot(layer, layout, AMBER_KIT_CX, HUD_CY, 3.5f, dim);
}

static void draw_maneuver(lv_layer_t *layer, const ui_layout_t *layout,
                          const companion_nav_t *nav) {
    bool mirror;
    const route_shape_t shape = shape_of(nav->maneuver, &mirror);
    point_t pts[9];
    for (unsigned i = 0; i < shape.count; i++) {
        pts[i] = (point_t){mx(shape.points[i].x, mirror), shape.points[i].y};
    }

    const lv_color_t rail = ui_theme_amber_dim();
    const lv_color_t vein = ui_theme_amber_bright();
    const lv_color_t plot = ui_theme_amber_separator();

    // Passe 0 : gabarit de couloir éteint, bords francs (aucun arrondi, esprit
    // CAO). Passe 1 : vecteur de cap lumineux et incisif.
    for (unsigned pass = 0; pass < 2U; pass++) {
        const bool rail_pass = pass == 0U;
        const float width = rail_pass ? ROUTE_RAIL_W : ROUTE_CORE_W;
        const lv_color_t color = rail_pass ? rail : vein;
        for (unsigned i = 0; i + 1U < shape.count; i++) {
            amber_draw_line(layer, layout, pts[i].x, pts[i].y, pts[i + 1U].x,
                            pts[i + 1U].y, width, color, false);
        }
        // Sommets : pastille de jonction (le trait à bouts francs laisserait
        // une entaille dans les coudes) et plots de repère posés sur le rail,
        // sous la veine lumineuse (un point sombre sur la veine ferait tache).
        for (unsigned i = 1; i + 1U < shape.count; i++) {
            amber_draw_dot(layer, layout, pts[i].x, pts[i].y, width * 0.5f,
                           color);
            if (rail_pass) {
                amber_draw_dot(layer, layout, pts[i].x, pts[i].y, 2.4f, plot);
            }
        }

        if (shape.ring) {
            // Rond-point (circulation à droite) : la chaussée dessine l'anneau
            // complet, le vecteur n'en suit que la portion parcourue (entrée
            // en bas, sortie en haut à droite). Centre abaissé et rayon
            // resserré pour dégager la sortie sous le titre NAVIGATION.
            const float r = RING_R;
            if (rail_pass) {
                amber_draw_circle(layer, layout, AMBER_KIT_CX, RING_CY,
                                  r + width * 0.5f, width, color);
            } else {
                amber_draw_arc_wrapped(layer, layout, AMBER_KIT_CX, RING_CY,
                                       r + width * 0.5f, width, 315.0f, 135.0f,
                                       color, false);
            }
            const point_t exit_a = {AMBER_KIT_CX + r * 0.7071f,
                                    RING_CY - r * 0.7071f};
            const point_t exit_b = {190.0f, 67.0f};
            amber_draw_line(layer, layout, exit_a.x, exit_a.y, exit_b.x,
                            exit_b.y, width, color, false);
            if (!rail_pass) draw_head(layer, layout, exit_a, exit_b, color);
        } else if (shape.pin) {
            const point_t end = pts[shape.count - 1U];
            if (rail_pass) {
                draw_octagon(layer, layout, end.x, end.y - 9.0f, 13.0f, 1.1f,
                             rail);
            }
        } else if (shape.count >= 2U) {
            draw_head(layer, layout, pts[shape.count - 2U],
                      pts[shape.count - 1U], color);
        }
    }

    if (shape.pin) {
        // Arrivée : cible d'acquisition (losange + point) plutôt qu'une
        // pastille pleine.
        const point_t end = pts[shape.count - 1U];
        draw_diamond(layer, layout, end.x, end.y - 9.0f, 8.0f, 1.8f,
                     ui_theme_amber_bright());
        amber_draw_dot(layer, layout, end.x, end.y - 9.0f, 2.4f,
                       ui_theme_amber_bright());
    }
}

static void draw_route(lv_layer_t *layer, const ui_layout_t *layout,
                       void *context) {
    const navigation_screen_t *scr = context;
    if (scr == NULL || !scr->linked || !scr->nav.active) {
        draw_radar_mire(layer, layout);
        return;
    }
    draw_maneuver(layer, layout, &scr->nav);
}

static void refresh(navigation_screen_t *scr) {
    const companion_nav_t *nav = &scr->nav;
    // Sans téléphone lié, aucun guidage ne doit s'afficher même si un état
    // `nav` périmé traîne avec active == true.
    if (!scr->linked || !nav->active) {
        amber_page_set_hero(&scr->page, "--", false);
        amber_readout_set_unit(&scr->page.hero, "");
        if (amber_kit_set_text(scr->instruction,
                               scr->linked ? "AUCUN GUIDAGE" : "TELEPHONE")) {
            amber_kit_place(scr->instruction, AMBER_KIT_CX, INSTRUCTION_Y,
                            AMBER_ALIGN_CENTER);
        }
        lv_obj_set_style_text_color(scr->instruction,
                                    ui_theme_amber_separator(), 0);
        if (amber_kit_set_text(scr->street,
                               scr->linked ? "" : "NON CONNECTE")) {
            amber_kit_place(scr->street, AMBER_KIT_CX, STREET_Y,
                            AMBER_ALIGN_CENTER);
        }
        lv_obj_set_style_text_color(scr->street, ui_theme_amber_separator(), 0);
        amber_page_set_status(&scr->page, "", false);
        amber_page_invalidate(&scr->page);
        return;
    }

    // Une distance numérique commence par un chiffre ; sinon la consigne est
    // immédiate (« MAINTENANT ») et vaut zéro mètre.
    const bool numeric =
        nav->distance_value[0] >= '0' && nav->distance_value[0] <= '9';

    // Une consigne immédiate (« MAINTENANT ») est une distance nulle, pas une
    // mesure indisponible : elle s'affiche en ambre vif.
    amber_page_set_hero(&scr->page, numeric ? nav->distance_value : "0", true);
    amber_readout_set_unit(&scr->page.hero, numeric ? nav->distance_unit : "M");

    const char *instruction = nav->instruction[0] != '\0' ? nav->instruction
                              : (!numeric && nav->distance_value[0] != '\0')
                                  ? nav->distance_value
                                  : "SUIVRE L'ITINERAIRE";
    if (amber_kit_set_text(scr->instruction, instruction)) {
        amber_kit_place(scr->instruction, AMBER_KIT_CX, INSTRUCTION_Y,
                        AMBER_ALIGN_CENTER);
    }
    lv_obj_set_style_text_color(scr->instruction, ui_theme_amber_bright(), 0);
    // La voie est la cible recherchée des yeux : ambre vif, pleine largeur
    // disponible à cette hauteur (corde du cadran).
    if (amber_kit_set_text(scr->street, nav->street)) {
        amber_kit_place(scr->street, AMBER_KIT_CX, STREET_Y,
                        AMBER_ALIGN_CENTER);
    }
    lv_obj_set_style_text_color(scr->street, ui_theme_amber_bright(), 0);
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

    // La valeur héros descend sous la zone de manœuvre.
    scr->page.hero.y = DISTANCE_Y;
    amber_readout_layout(&scr->page.hero);

    lv_obj_t *root = scr->page.root;
    const ui_layout_t layout =
        ui_layout_fit(lv_obj_get_width(root), lv_obj_get_height(root));
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
    if (!scr->linked || nav == NULL) {
        memset(&scr->nav, 0, sizeof(scr->nav));
    } else {
        scr->nav = *nav;
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
