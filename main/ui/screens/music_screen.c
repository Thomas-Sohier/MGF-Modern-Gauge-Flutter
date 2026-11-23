#include "ui/screens/music_screen.h"

#include <string.h>

#include "domain/music_cover.h"
#include "ui/screens/music_cover_fit.h"
#include "src/misc/cache/lv_image_cache.h"
#include "ui/themes/ui_theme.h"
#include "ui/ui_layout.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_kit.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Lecture en cours sur le téléphone (application compagnon). Le lien BLE est à
// sens unique (téléphone -> jauge) : la pochette JPEG est réassemblée puis
// décodée hors LVGL (companion_art_worker) et déposée ici en RGB565 ambre. En
// l'absence d'image valide, une cassette audio vectorielle à blocs pixel sert
// de repli (aucun aplat ambre plein : de nuit, il serait aveuglant).
//
// Composition : anneau de progression continu proche du bord (piste sombre,
// départ à 12 h, sens horaire), pochette en cadrage « cover » (échelle
// uniforme, rognage centré) sur toute la largeur intérieure de l'anneau et
// jusqu'au titre, découpée par l'arc intérieur de l'anneau, puis
// titre, artiste et durée totale. Aucun header, bouton ni pictogramme de
// transport : l'état figé (pause/arrêt) se lit uniquement au fait que l'anneau
// cesse d'avancer.
// Géométrie (repère 320) et cadrage « cover » : music_cover_fit.h. La
// pochette remplit toute la largeur intérieure de l'anneau jusqu'au titre ;
// son parent est un disque tangent au bord intérieur de l'anneau, qui découpe
// les côtés de l'image selon le même arc que la progression.
#define RING_R         MUSIC_RING_R
#define RING_W         MUSIC_RING_W
#define COVER_CLIP_XY  MUSIC_COVER_CLIP_XY
#define COVER_CLIP_D   MUSIC_COVER_CLIP_D
#define COVER_IMAGE_Y1 MUSIC_COVER_IMAGE_Y1
#define TRACK_Y        MUSIC_TRACK_Y
#define ARTIST_Y       MUSIC_ARTIST_Y
#define RULE_Y         MUSIC_RULE_Y
#define TIME_Y         MUSIC_TIME_Y
#define TRACK_WIDTH    MUSIC_TRACK_WIDTH
#define ARTIST_WIDTH   MUSIC_ARTIST_WIDTH

struct music_screen_s {
    amber_page_t page;
    lv_obj_t *cover;          // disque qui découpe les côtés selon l'anneau
    lv_obj_t *cover_viewport; // coupe horizontale avant le titre
    lv_obj_t *cover_img;      // image RGB565 ambre
    lv_obj_t *ring;           // calque de l'anneau, au-dessus de la pochette
    lv_image_dsc_t cover_dsc;
    uint16_t *cover_pixels; // possédé par l'écran
    int cover_width;
    int cover_height;
    lv_obj_t *track;
    lv_obj_t *artist;
    lv_obj_t *duration;
    companion_media_t media;
    uint32_t received_ms;
    uint32_t anim_ms; // horloge de la rotation 3D (cassette de repli)
    bool has_media;
    bool linked;
    float progress;        // 0..1, ou < 0 si inconnue
    int64_t shown_seconds; // dernière position affichée
};

// ── Pochette ────────────────────────────────────────────────────────────────
// Repli sans pochette : cassette audio rétro-futuriste en 2.5D, fil de fer
// tramé de blocs « pixel » (grille ASCII, vis, encoches, cartouche matriciel).
// Le contraste reste contenu — un aplat ambre plein serait aveuglant de nuit.
// La cassette tourne en permanence autour de son axe vertical (avec ou sans
// musique) et les bobines suivent la progression. L'horloge `anim_ms` vaut 0
// au simulateur : pose de repos déterministe pour les golden.
//
// Repère local de la cassette : u ∈ [-82, +82] (largeur), v ∈ [-52, +52]
// (vers le bas), z ∈ [-11, +11] (épaisseur, z > 0 vers l'observateur). Le
// projecteur applique un lacet θ (rotation autour de l'axe vertical), une
// plongée fixe φ = 13° qui découvre la tranche supérieure et l'épaisseur du
// boîtier, puis une perspective douce centrée sur la fenêtre de pochette.
#define CASSETTE_CX        160.0f
#define CASSETTE_CY        115.0f
#define CASSETTE_HALF_W    82.0f
#define CASSETTE_HALF_H    52.0f
#define CASSETTE_HALF_Z    11.0f
#define CASSETTE_FOV       380.0f
#define CASSETTE_PITCH_DEG 13.0f
#define CASSETTE_REST_YAW  (-26.0f)
#define CASSETTE_SPIN_MS   12000

typedef struct {
    float cos_yaw;
    float sin_yaw;
    float cos_pitch;
    float sin_pitch;
} music_proj_t;

typedef struct {
    float x;
    float y;
    float k; // facteur de perspective (1 = plan neutre)
} music_pt_t;

static void draw_pixel_block(lv_layer_t *layer, const ui_layout_t *layout,
                             float cx, float cy, float w, float h,
                             lv_color_t color) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = 0;
    const lv_area_t area = {
        (int32_t)lroundf(ui_layout_x(layout, cx - w * 0.5f)),
        (int32_t)lroundf(ui_layout_y(layout, cy - h * 0.5f)),
        (int32_t)lroundf(ui_layout_x(layout, cx + w * 0.5f)),
        (int32_t)lroundf(ui_layout_y(layout, cy + h * 0.5f)),
    };
    lv_draw_rect(layer, &dsc, &area);
}

// Projection perspective : rotation Y (lacet), puis rotation X (plongée),
// puis division perspective. z2 > 0 rapproche l'observateur.
static music_pt_t project_3d(const music_proj_t *proj, float u, float v,
                             float z) {
    const float x1 = u * proj->cos_yaw + z * proj->sin_yaw;
    const float z1 = -u * proj->sin_yaw + z * proj->cos_yaw;
    const float y2 = v * proj->cos_pitch - z1 * proj->sin_pitch;
    const float z2 = v * proj->sin_pitch + z1 * proj->cos_pitch;
    const float k = CASSETTE_FOV / (CASSETTE_FOV - z2);
    return (music_pt_t){
        .x = CASSETTE_CX + x1 * k, .y = CASSETTE_CY + y2 * k, .k = k};
}

static void draw_line_3d(lv_layer_t *layer, const ui_layout_t *layout,
                         const music_proj_t *proj, float u1, float v1, float z1,
                         float u2, float v2, float z2, float width,
                         lv_color_t color) {
    const music_pt_t a = project_3d(proj, u1, v1, z1);
    const music_pt_t b = project_3d(proj, u2, v2, z2);
    amber_draw_line(layer, layout, a.x, a.y, b.x, b.y, width, color, false);
}

static void draw_pixel_3d(lv_layer_t *layer, const ui_layout_t *layout,
                          const music_proj_t *proj, float u, float v, float z,
                          float size, lv_color_t color) {
    const music_pt_t p = project_3d(proj, u, v, z);
    draw_pixel_block(layer, layout, p.x, p.y, size * p.k, size * p.k, color);
}

// Cercle 3D : projeté, il devient une ellipse en perspective. `phase_deg`
// décale le point de départ, ce qui anime les pointillés de la bande.
static void draw_circle_3d(lv_layer_t *layer, const ui_layout_t *layout,
                           const music_proj_t *proj, float cu, float cv,
                           float cz, float radius, int segments,
                           float phase_deg, float width, lv_color_t color,
                           bool dashed) {
    if (segments < 3) segments = 3;
    const float step = 2.0f * (float)M_PI / (float)segments;
    const float phase = phase_deg * (float)M_PI / 180.0f;
    music_pt_t prev = project_3d(proj, cu + radius * cosf(phase),
                                 cv + radius * sinf(phase), cz);
    for (int i = 1; i <= segments; i++) {
        const float angle = phase + (float)i * step;
        const music_pt_t cur = project_3d(proj, cu + radius * cosf(angle),
                                          cv + radius * sinf(angle), cz);
        // Un segment sur deux, décalés par la phase : la bande « tourne ».
        if (!dashed || (i & 1) == 0) {
            amber_draw_line(layer, layout, prev.x, prev.y, cur.x, cur.y, width,
                            color, false);
        }
        prev = cur;
    }
}

// Boîtier chanfreiné 164 x 104 (chanfrein 7), sommets dans le sens horaire.
static const float k_cassette_octagon[8][2] = {
    {-75.0f, -52.0f}, {75.0f, -52.0f}, {82.0f, -45.0f}, {82.0f, 45.0f},
    {75.0f, 52.0f},   {-75.0f, 52.0f}, {-82.0f, 45.0f}, {-82.0f, -45.0f},
};

static void draw_octagon_3d(lv_layer_t *layer, const ui_layout_t *layout,
                            const music_proj_t *proj, float z, float width,
                            lv_color_t color) {
    for (int i = 0; i < 8; i++) {
        const int n = (i + 1) & 7;
        draw_line_3d(layer, layout, proj, k_cassette_octagon[i][0],
                     k_cassette_octagon[i][1], z, k_cassette_octagon[n][0],
                     k_cassette_octagon[n][1], z, width, color);
    }
}

// Rectangle chanfreiné (étiquette, cartouche) sur un plan z donné.
static void draw_chamfer_3d(lv_layer_t *layer, const ui_layout_t *layout,
                            const music_proj_t *proj, float u0, float v0,
                            float u1, float v1, float chamfer, float z,
                            float width, lv_color_t color) {
    const float pts[8][2] = {
        {u0 + chamfer, v0}, {u1 - chamfer, v0}, {u1, v0 + chamfer},
        {u1, v1 - chamfer}, {u1 - chamfer, v1}, {u0 + chamfer, v1},
        {u0, v1 - chamfer}, {u0, v0 + chamfer},
    };
    for (int i = 0; i < 8; i++) {
        const int n = (i + 1) & 7;
        draw_line_3d(layer, layout, proj, pts[i][0], pts[i][1], z, pts[n][0],
                     pts[n][1], z, width, color);
    }
}

// Bobine posée à l'intérieur de la fenêtre : bande pointillée qui tourne,
// sillon continu, moyeu vif, cylindre de profondeur et six dents.
static void draw_reel_3d(lv_layer_t *layer, const ui_layout_t *layout,
                         const music_proj_t *proj, float cu, float z_face,
                         float r_tape, float phase_deg, lv_color_t bright,
                         lv_color_t dim) {
    const float cv = -2.0f;
    const float z_reel = z_face * 0.65f;
    const float z_deep = z_face * 0.25f;

    // Bande magnétique : anneau pointillé (r_tape = bande restante) et sillon
    // continu à mi-course.
    draw_circle_3d(layer, layout, proj, cu, cv, z_reel + 0.6f, r_tape, 16,
                   phase_deg, 1.2f, dim, true);
    draw_circle_3d(layer, layout, proj, cu, cv, z_reel, (r_tape + 12.5f) * 0.5f,
                   16, 0.0f, 1.0f, dim, false);

    draw_circle_3d(layer, layout, proj, cu, cv, z_reel, 10.5f, 16, 0.0f, 1.8f,
                   bright, false);
    // Anneau de profondeur plus petit : le moyeu se lit comme un cylindre.
    draw_circle_3d(layer, layout, proj, cu, cv, z_deep, 8.0f, 12, 0.0f, 1.0f,
                   dim, false);

    for (int d = 0; d < 6; d++) {
        const float angle = phase_deg + (float)(d * 60);
        const float rad = angle * (float)M_PI / 180.0f;
        const float ca = cosf(rad);
        const float sa = sinf(rad);
        draw_line_3d(layer, layout, proj, cu + ca * 10.5f, cv + sa * 10.5f,
                     z_reel, cu + ca * 8.0f, cv + sa * 8.0f, z_deep, 1.0f, dim);
        if ((d & 1) == 0) {
            draw_pixel_3d(layer, layout, proj, cu + ca * 7.0f, cv + sa * 7.0f,
                          z_reel, 2.6f, bright);
        } else {
            draw_line_3d(layer, layout, proj, cu + ca * 6.5f, cv + sa * 6.5f,
                         z_reel, cu + ca * 11.5f, cv + sa * 11.5f, z_reel, 2.2f,
                         bright);
        }
    }
}

// Lacet de la cassette : rotation continue d'un tour toutes les 12 s sur
// l'horloge d'animation, que de la musique soit lue ou non (anim_ms = 0 ->
// pose 3/4 de repos).
static float cover_yaw_deg(const music_screen_t *scr) {
    if (scr == NULL) return CASSETTE_REST_YAW;
    const uint32_t phase = scr->anim_ms % (uint32_t)CASSETTE_SPIN_MS;
    return CASSETTE_REST_YAW + (float)phase * 360.0f / (float)CASSETTE_SPIN_MS;
}

static void draw_cover_fallback(lv_layer_t *layer, const ui_layout_t *layout,
                                const music_screen_t *scr) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t separator = ui_theme_amber_separator();

    const float yaw_deg = cover_yaw_deg(scr);
    const float yaw = yaw_deg * (float)M_PI / 180.0f;
    const float pitch = CASSETTE_PITCH_DEG * (float)M_PI / 180.0f;
    const music_proj_t proj = {
        .cos_yaw = cosf(yaw),
        .sin_yaw = sinf(yaw),
        .cos_pitch = cosf(pitch),
        .sin_pitch = sinf(pitch),
    };
    const float zf =
        (proj.cos_yaw >= 0.0f) ? CASSETTE_HALF_Z : -CASSETTE_HALF_Z;
    const float zb = -zf;
    // Flanc réellement tourné vers la caméra : sa normale (+u) projetée sur z
    // vaut -sin(θ), elle n'est positive que pour le côté opposé au signe.
    const float u_side =
        (proj.sin_yaw >= 0.0f) ? -CASSETTE_HALF_W : CASSETTE_HALF_W;

    // 1. Podium télémétrique : ellipse d'ombre en blocs espacés sous la
    // cassette, et deux croix de visée « scanner CAO » de part et d'autre.
    for (int i = 0; i < 26; i++) {
        const float angle = (float)i * 2.0f * (float)M_PI / 26.0f;
        draw_pixel_block(layer, layout, CASSETTE_CX + cosf(angle) * 72.0f,
                         186.0f + sinf(angle) * 8.0f, 3.0f, 2.2f, dim);
    }
    for (int side = 0; side < 2; side++) {
        const float x = (side == 0) ? 64.0f : 256.0f;
        draw_pixel_block(layer, layout, x, CASSETTE_CY, 1.4f, 9.0f, dim);
        draw_pixel_block(layer, layout, x, CASSETTE_CY, 9.0f, 1.4f, dim);
    }

    // 2. Face arrière du boîtier, en fil de fer sombre.
    draw_octagon_3d(layer, layout, &proj, zb, 1.0f, dim);

    // 3. Tranche 3D : arêtes de liaison, tramage pixel du flanc visible et de
    // la tranche supérieure (découverte par la plongée de 13°).
    for (int i = 0; i < 8; i++) {
        draw_line_3d(layer, layout, &proj, k_cassette_octagon[i][0],
                     k_cassette_octagon[i][1], zb, k_cassette_octagon[i][0],
                     k_cassette_octagon[i][1], zf, 1.3f, separator);
    }
    for (int row = 0; row < 9; row++) {
        const float v = -36.0f + (float)row * 9.0f;
        for (int depth = 0; depth < 3; depth++) {
            const float z = -5.5f + (float)depth * 5.5f;
            const lv_color_t color = ((row + depth) & 1) ? separator : dim;
            draw_pixel_3d(layer, layout, &proj, u_side, v, z, 2.6f, color);
        }
    }
    for (int col = 0; col < 9; col++) {
        draw_pixel_3d(layer, layout, &proj, -64.0f + (float)col * 16.0f,
                      -CASSETTE_HALF_H, 0.0f, 2.6f, dim);
    }

    // 4. Trapèze du pont de tête de lecture : sur-épaisseur vers l'avant,
    // reliée à la face par quatre arêtes 3D.
    const float z_bridge = zf * 1.32f;
    static const float bridge[4][2] = {
        {-48.0f, 52.0f},
        {-41.0f, 27.0f},
        {41.0f, 27.0f},
        {48.0f, 52.0f},
    };
    for (int i = 0; i < 4; i++) {
        const int n = (i + 1) & 3;
        draw_line_3d(layer, layout, &proj, bridge[i][0], bridge[i][1], z_bridge,
                     bridge[n][0], bridge[n][1], z_bridge, 1.6f,
                     (i == 1) ? bright : separator);
        draw_line_3d(layer, layout, &proj, bridge[i][0], bridge[i][1], zf,
                     bridge[i][0], bridge[i][1], z_bridge, 1.0f, dim);
    }

    // Cabestans et fentes d'entraînement pixelisées.
    for (int k = 0; k < 2; k++) {
        const float cu = (k == 0) ? -26.0f : 26.0f;
        draw_circle_3d(layer, layout, &proj, cu, 41.0f, z_bridge, 4.2f, 12,
                       0.0f, 1.2f, dim, false);
        draw_pixel_3d(layer, layout, &proj, cu, 41.0f, z_bridge + 0.4f, 1.8f,
                      bright);
    }
    for (int k = 0; k < 2; k++) {
        const float cu = (k == 0) ? -10.0f : 10.0f;
        draw_pixel_3d(layer, layout, &proj, cu, 41.0f, z_bridge + 0.3f, 2.2f,
                      dim);
        draw_pixel_3d(layer, layout, &proj, cu, 43.5f, z_bridge + 0.3f, 2.2f,
                      bright);
    }

    // 5. Face avant : boîtier, vis de coin, étiquette ASCII, fenêtre.
    draw_octagon_3d(layer, layout, &proj, zf, 1.8f, separator);
    for (int k = 0; k < 4; k++) {
        const float u = (k & 1) ? 70.0f : -70.0f;
        const float v = (k & 2) ? 41.0f : -41.0f;
        draw_pixel_3d(layer, layout, &proj, u, v, zf + 0.2f, 3.5f, dim);
        draw_pixel_3d(layer, layout, &proj, u, v, zf + 0.5f, 1.2f, bright);
    }

    draw_chamfer_3d(layer, layout, &proj, -70.0f, -43.0f, 70.0f, 18.0f, 4.0f,
                    zf, 1.1f, dim);

    // Trame matricielle de l'étiquette : matrice 4x2 à gauche, deux filets au
    // centre, badge carré ajouré à droite.
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 4; col++) {
            draw_pixel_3d(layer, layout, &proj, -62.0f + (float)col * 8.0f,
                          -36.0f + (float)row * 6.0f, zf + 0.2f, 2.5f,
                          separator);
        }
    }
    draw_line_3d(layer, layout, &proj, -28.0f, -36.0f, zf, 42.0f, -36.0f, zf,
                 1.0f, dim);
    draw_line_3d(layer, layout, &proj, -28.0f, -30.0f, zf, 42.0f, -30.0f, zf,
                 1.0f, dim);
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 2; col++) {
            draw_pixel_3d(layer, layout, &proj, 52.0f + (float)col * 6.5f,
                          -36.0f + (float)row * 6.0f, zf + 0.2f, 2.5f, bright);
        }
    }

    // Fenêtre panoramique des bobines.
    draw_line_3d(layer, layout, &proj, -58.0f, -22.0f, zf, 58.0f, -22.0f, zf,
                 1.3f, separator);
    draw_line_3d(layer, layout, &proj, -58.0f, 18.0f, zf, 58.0f, 18.0f, zf,
                 1.3f, separator);
    draw_line_3d(layer, layout, &proj, -58.0f, -22.0f, zf, -58.0f, 18.0f, zf,
                 1.3f, separator);
    draw_line_3d(layer, layout, &proj, 58.0f, -22.0f, zf, 58.0f, 18.0f, zf,
                 1.3f, separator);

    // Échelle de bande centrale : cinq graduations, et deux pistes de pixels
    // qui alternent au fil de la rotation.
    const int scroll = (int)((yaw_deg - CASSETTE_REST_YAW) / 30.0f) & 1;
    for (int k = 0; k < 5; k++) {
        draw_pixel_3d(layer, layout, &proj, -8.0f + (float)k * 4.0f, -2.0f,
                      zf + 0.2f, (k == 2) ? 3.2f : 2.0f, separator);
    }
    for (int k = 0; k < 6; k++) {
        const float u = -10.0f + (float)k * 4.0f;
        const bool on = (((k + scroll) & 1) == 0);
        draw_pixel_3d(layer, layout, &proj, u, -14.0f, zf + 0.2f, 2.5f,
                      on ? bright : dim);
        draw_pixel_3d(layer, layout, &proj, u, 10.0f, zf + 0.2f, 2.5f,
                      on ? dim : bright);
    }

    // Répartition dynamique de la bande : la gauche se vide pendant que la
    // droite se remplit (progression connue), sinon valeurs fixes.
    float r_tape_l = 18.0f;
    float r_tape_r = 14.0f;
    if (scr != NULL && scr->progress >= 0.0f) {
        const float p = amber_clampf(scr->progress, 0.0f, 1.0f);
        r_tape_l = 19.0f - 6.0f * p;
        r_tape_r = 13.0f + 6.0f * p;
    }
    // Les bobines tournent quatre fois plus vite que la cassette, soit une
    // révolution toutes les 3 s.
    const float phase_deg = yaw_deg * 4.0f;
    draw_reel_3d(layer, layout, &proj, -37.0f, zf, r_tape_l, phase_deg, bright,
                 dim);
    draw_reel_3d(layer, layout, &proj, 37.0f, zf, r_tape_r, phase_deg, bright,
                 dim);
}

static void draw_state(lv_layer_t *layer, const ui_layout_t *layout,
                       void *context) {
    music_screen_t *scr = context;
    if (scr == NULL) return;

    // Remplissage de repli sous l'objet image : le canvas est le seul à le
    // dessiner quand aucune pochette réelle n'est prête.
    if (scr->cover_pixels == NULL) {
        draw_cover_fallback(layer, layout, scr);
    }

    amber_draw_line(layer, layout, 112.0f, RULE_Y, 208.0f, RULE_Y, 1.0f,
                    ui_theme_amber_separator(), false);
}

// L'anneau est dessiné sur un calque dédié, au-dessus de la pochette (il borde
// l'image qui occupe toute la largeur) mais sous les textes.
static void ring_draw_cb(lv_event_t *event) {
    lv_obj_t *canvas = lv_event_get_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    music_screen_t *scr = lv_obj_get_user_data(canvas);
    if (scr == NULL || layer == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();

    // Anneau continu : piste complète sombre, progression éventuelle en ambre
    // vif depuis 12 h (angle 270°), dans le sens horaire.
    amber_draw_circle(layer, &layout, AMBER_KIT_CX, AMBER_KIT_CX, RING_R,
                      RING_W, dim);
    if (scr->progress > 0.0f) {
        amber_draw_arc_wrapped(layer, &layout, AMBER_KIT_CX, AMBER_KIT_CX,
                               RING_R, RING_W, 270.0f, 360.0f * scr->progress,
                               bright, false);
    }
}

static void format_time(char *out, size_t size, int64_t ms) {
    if (ms < 0) {
        snprintf(out, size, "--:--");
        return;
    }
    const int64_t total = ms / 1000;
    const int64_t hours = total / 3600;
    if (hours > 0) {
        snprintf(out, size, "%d:%02d:%02d", (int)hours,
                 (int)((total / 60) % 60), (int)(total % 60));
    } else {
        snprintf(out, size, "%02d:%02d", (int)(total / 60), (int)(total % 60));
    }
}

static void place_duration(music_screen_t *scr) {
    amber_kit_place(scr->duration, AMBER_KIT_CX, TIME_Y, AMBER_ALIGN_CENTER);
}

// Recalcule position, durée et progression ; n'invalide que si l'affichage
// change (une fois par seconde en lecture).
static void refresh_progress(music_screen_t *scr, uint32_t now_ms) {
    const bool active = scr->has_media && scr->linked;
    const int64_t position =
        active
            ? companion_media_position_at(&scr->media, scr->received_ms, now_ms)
            : -1;
    const int64_t duration =
        active && scr->media.duration_ms > 0 ? scr->media.duration_ms : -1;
    const int64_t seconds = position < 0 ? -1 : position / 1000;
    // Quantifiée à la seconde : un seul redessin par seconde en lecture.
    const float progress =
        seconds >= 0 && duration > 0
            ? (float)((double)(seconds * 1000) / (double)duration)
            : -1.0f;

    // Position courante et durée totale côte à côte ; l'anneau porte la
    // progression. Hors lecture ou durée inconnue, seule la durée s'affiche
    // (« --:-- » si déconnecté).
    char text[40]; // 2 x 16 (format_time) + " / " : pas de troncature
    if (active && seconds >= 0 && duration > 0) {
        char pos_text[16];
        char dur_text[16];
        format_time(pos_text, sizeof(pos_text), seconds * 1000);
        format_time(dur_text, sizeof(dur_text), duration);
        snprintf(text, sizeof(text), "%s / %s", pos_text, dur_text);
    } else {
        format_time(text, sizeof(text), duration);
    }
    if (amber_kit_set_text(scr->duration, text)) place_duration(scr);

    if (seconds != scr->shown_seconds || progress != scr->progress) {
        scr->shown_seconds = seconds;
        scr->progress = progress;
        // Seuls l'anneau et le libellé de temps changent : invalider la page
        // entière forçait LVGL à redessiner le fond sous la pochette chaque
        // seconde (coûteux sur ESP32-S3).
        if (scr->ring != NULL) lv_obj_invalidate(scr->ring);
        // Sans pochette bitmap, la cassette de repli anime ses bobines et sa
        // rotation : si le tick de 80 ms ne l'a pas déjà invalidée sur ce
        // cycle, on redessine la page au changement de seconde.
        if (scr->cover_pixels == NULL && scr->anim_ms != now_ms) {
            amber_page_invalidate(&scr->page);
        }
    }
}

static void refresh_texts(music_screen_t *scr) {
    const bool active = scr->has_media && scr->linked;
    const char *title = !scr->linked                  ? "TELEPHONE"
                        : !scr->has_media             ? "AUCUN MEDIA"
                        : scr->media.title[0] != '\0' ? scr->media.title
                                                      : "SANS TITRE";
    if (amber_kit_set_text(scr->track, title)) {
        amber_kit_place(scr->track, AMBER_KIT_CX, TRACK_Y, AMBER_ALIGN_CENTER);
    }
    lv_obj_set_style_text_color(
        scr->track,
        active ? ui_theme_amber_bright() : ui_theme_amber_separator(), 0);

    const char *artist = !scr->linked ? "NON CONNECTE"
                         : active     ? scr->media.artist
                                      : "";
    if (amber_kit_set_text(scr->artist, artist)) {
        amber_kit_place(scr->artist, AMBER_KIT_CX, ARTIST_Y,
                        AMBER_ALIGN_CENTER);
    }
}

void music_screen_set_cover(music_screen_t *scr, uint16_t *pixels, int width,
                            int height) {
    if (scr == NULL) return;
    // Le descripteur garde la même adresse : on purge l'entrée de cache qui
    // pourrait référencer l'ancien tampon avant de le libérer.
    lv_image_cache_drop(&scr->cover_dsc);
    if (scr->cover_pixels != NULL) lv_free(scr->cover_pixels);
    scr->cover_pixels = NULL;
    scr->cover_width = 0;
    scr->cover_height = 0;

    if (pixels != NULL && width > 0 && height > 0 && scr->cover != NULL &&
        scr->cover_viewport != NULL && scr->cover_img != NULL) {
        scr->cover_pixels = pixels;
        scr->cover_width = width;
        scr->cover_height = height;
        scr->cover_dsc = (lv_image_dsc_t){
            .header =
                {
                    .magic = LV_IMAGE_HEADER_MAGIC,
                    .cf = LV_COLOR_FORMAT_RGB565,
                    .w = (uint32_t)width,
                    .h = (uint32_t)height,
                    .stride = (uint32_t)width * 2U,
                },
            .data_size = (uint32_t)width * (uint32_t)height * 2U,
            .data = (const uint8_t *)pixels,
        };
        // Cadrage « cover » : l'objet image a la taille exacte de la fenêtre
        // et centre le bitmap ; une échelle uniforme (pivot au centre) le
        // grossit jusqu'à couvrir largeur ET hauteur. Le dépassement est
        // rogné symétriquement par la fenêtre, sans déformation, quelles que
        // soient les proportions de la source.
        const int32_t box_w = lv_obj_get_width(scr->cover_viewport);
        const int32_t box_h = lv_obj_get_height(scr->cover_viewport);
        music_cover_fit_t fit;
        if (!music_cover_fit(width, height, box_w, box_h, &fit)) {
            fit.scale = LV_SCALE_NONE;
        }
        lv_image_set_src(scr->cover_img, &scr->cover_dsc);
        lv_obj_set_size(scr->cover_img, box_w, box_h);
        lv_image_set_inner_align(scr->cover_img, LV_IMAGE_ALIGN_CENTER);
        lv_image_set_pivot(scr->cover_img, width / 2, height / 2);
        lv_image_set_scale(scr->cover_img, fit.scale);
        lv_obj_center(scr->cover_img);
        lv_obj_remove_flag(scr->cover_img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(scr->cover, LV_OBJ_FLAG_HIDDEN);
    } else {
        if (pixels != NULL) lv_free(pixels);
        if (scr->cover_img != NULL) {
            lv_obj_add_flag(scr->cover_img, LV_OBJ_FLAG_HIDDEN);
        }
        if (scr->cover != NULL) {
            lv_obj_add_flag(scr->cover, LV_OBJ_FLAG_HIDDEN);
        }
    }
    amber_page_invalidate(&scr->page);
}

void music_screen_palette_changed(music_screen_t *scr, bool from_inverted,
                                  bool to_inverted) {
    if (scr == NULL || from_inverted == to_inverted) return;
    if (scr->cover_pixels != NULL) {
        music_cover_convert_palette(scr->cover_pixels,
                                    (size_t)scr->cover_width *
                                        (size_t)scr->cover_height,
                                    from_inverted, to_inverted);
        lv_image_cache_drop(&scr->cover_dsc);
        lv_obj_invalidate(scr->cover_img);
    }
}

music_screen_t *music_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;
    music_screen_t *scr = lv_malloc(sizeof(*scr));
    if (scr == NULL) return NULL;
    lv_memzero(scr, sizeof(*scr));
    scr->progress = -1.0f;
    scr->shown_seconds = -2;

    const amber_page_spec_t spec = {
        .title = "",
        .rim = AMBER_RIM_NONE,
        .no_grid = true,
        .draw_extra = draw_state,
        .context = scr,
    };
    if (!amber_page_create(&scr->page, parent, &spec)) goto fail;
    // Le gabarit fournit une valeur héros inutile ici : on la masque.
    lv_obj_add_flag(scr->page.hero.value, LV_OBJ_FLAG_HIDDEN);

    // Disque de clipping tangent au bord intérieur de l'anneau. Le bitmap
    // n'occupe que sa partie haute, mais ses côtés sont ainsi découpés selon
    // l'arc du cadran. Non cliquable pour préserver les gestes du navigateur.
    lv_obj_t *root = scr->page.root;
    const ui_layout_t layout =
        ui_layout_fit(lv_obj_get_width(root), lv_obj_get_height(root));
    const int32_t cover_d = (int32_t)lroundf(COVER_CLIP_D * layout.scale);
    scr->cover = lv_obj_create(root);
    if (scr->cover == NULL) goto fail;
    lv_obj_remove_style_all(scr->cover);
    lv_obj_set_size(scr->cover, cover_d, cover_d);
    lv_obj_set_pos(scr->cover,
                   (int32_t)lroundf(ui_layout_x(&layout, COVER_CLIP_XY)),
                   (int32_t)lroundf(ui_layout_y(&layout, COVER_CLIP_XY)));
    lv_obj_set_style_radius(scr->cover, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_clip_corner(scr->cover, true, 0);
    // Fond transparent : sans image, le motif vectoriel du canvas reste visible.
    lv_obj_set_style_bg_opa(scr->cover, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(scr->cover,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    // Un second clipping, rectangulaire, arrête l'image juste avant le titre.
    // Le parent circulaire continue simultanément à découper ses côtés.
    scr->cover_viewport = lv_obj_create(scr->cover);
    if (scr->cover_viewport == NULL) goto fail;
    lv_obj_remove_style_all(scr->cover_viewport);
    lv_obj_set_size(
        scr->cover_viewport, cover_d,
        (int32_t)lroundf((COVER_IMAGE_Y1 - COVER_CLIP_XY) * layout.scale));
    lv_obj_align(scr->cover_viewport, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(scr->cover_viewport, 0, 0);
    lv_obj_set_style_clip_corner(scr->cover_viewport, true, 0);
    lv_obj_set_style_bg_opa(scr->cover_viewport, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(scr->cover_viewport,
                      LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    scr->cover_img = lv_image_create(scr->cover_viewport);
    if (scr->cover_img == NULL) goto fail;
    lv_obj_remove_style_all(scr->cover_img);
    lv_obj_clear_flag(scr->cover_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(scr->cover_img);
    lv_obj_add_flag(scr->cover_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(scr->cover, LV_OBJ_FLAG_HIDDEN);

    // Calque de l'anneau, créé après la pochette (pour border l'image) mais
    // avant les textes (pour rester dessous).
    scr->ring = amber_ui_canvas_create(root, scr, ring_draw_cb);
    if (scr->ring == NULL) goto fail;

    scr->track = amber_kit_label(root, amber_kit_font_value(),
                                 ui_theme_amber_bright(), "");
    scr->artist = amber_kit_caption(root, "");
    scr->duration = amber_kit_label(root, amber_kit_font_caption(),
                                    ui_theme_amber_separator(), "--:--");
    if (scr->track == NULL || scr->artist == NULL || scr->duration == NULL) {
        goto fail;
    }
    lv_obj_set_width(scr->track, (int32_t)lroundf(TRACK_WIDTH * layout.scale));
    lv_label_set_long_mode(scr->track, LV_LABEL_LONG_DOT);
    lv_obj_set_width(scr->artist,
                     (int32_t)lroundf(ARTIST_WIDTH * layout.scale));
    lv_label_set_long_mode(scr->artist, LV_LABEL_LONG_DOT);
    place_duration(scr);
    refresh_texts(scr);
    return scr;

fail:
    music_screen_destroy(scr);
    return NULL;
}

void music_screen_set_media(music_screen_t *scr, const companion_media_t *media,
                            uint32_t now_ms) {
    if (scr == NULL) return;
    // Une nouvelle pochette compagnon n'est envoyée que lorsque l'art_id
    // change ; l'ancienne image est donc abandonnée ici et remplacée par le
    // worker si un transfert suit. Même art_id (réémission de position) garde
    // l'image affichée. Un média effacé (déconnexion) doit en revanche purger
    // l'état complet : aucune pochette obsolète ne doit subsister au-dessus de
    // « TELEPHONE / NON CONNECTE », et la reconnexion sur le même morceau est
    // traitée comme un nouvel art_id (donc nouveau transfert).
    if (media == NULL || strcmp(scr->media.art_id, media->art_id) != 0) {
        music_screen_set_cover(scr, NULL, 0, 0);
    }
    scr->has_media = media != NULL;
    if (media != NULL) {
        scr->media = *media;
        scr->received_ms = now_ms;
    } else {
        memset(&scr->media, 0, sizeof(scr->media));
    }
    // Repart de l'instant de réception : la rotation 3D lit la position à
    // cette horloge (le tick ne la fait avancer que si la page l'anime).
    scr->anim_ms = now_ms;
    refresh_texts(scr);
    refresh_progress(scr, now_ms);
    amber_page_invalidate(&scr->page);
}

void music_screen_set_link(music_screen_t *scr, bool linked, uint32_t now_ms) {
    if (scr == NULL || scr->linked == linked) return;
    scr->linked = linked;
    if (!linked) {
        // La perte du téléphone efface média et pochette : rien ne doit rester
        // affiché, et le prochain état reçu repart d'art_id vide.
        music_screen_set_cover(scr, NULL, 0, 0);
        scr->has_media = false;
        memset(&scr->media, 0, sizeof(scr->media));
    }
    // Aligne l'horloge de rotation sur la (re)connexion.
    scr->anim_ms = now_ms;
    refresh_texts(scr);
    refresh_progress(scr, now_ms);
    amber_page_invalidate(&scr->page);
}

void music_screen_tick(music_screen_t *scr, uint32_t now_ms) {
    if (scr == NULL) return;
    // Cassette de repli (sans pochette) : la rotation 3D avance en permanence
    // à ~12,5 Hz, avec ou sans musique. La géométrie reste légère (primitives
    // vectorielles filaires), donc bornée pour l'ESP32-S3 ; page masquée =
    // invalidation sans redessin.
    if (scr->cover_pixels == NULL && now_ms - scr->anim_ms >= 80U) {
        scr->anim_ms = now_ms;
        amber_page_invalidate(&scr->page);
    }
    refresh_progress(scr, now_ms);
}

void music_screen_destroy(music_screen_t *scr) {
    if (scr == NULL) return;
    amber_page_destroy(&scr->page);
    if (scr->cover_pixels != NULL) lv_free(scr->cover_pixels);
    lv_free(scr);
}
