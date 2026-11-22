#include "ui/screens/boot_screen.h"

#include "ui/themes/ui_theme.h"
#include "ui/widgets/amber_draw.h"
#include "ui/widgets/amber_ui.h"

#include <math.h>
#include <stddef.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BOOT_CENTER 160.0f

// Octogone régulier (1:1) : la demi-longueur d'un côté droit vaut
// R * (sqrt(2) - 1).
#define OCT_SIDE_RATIO 0.41421356f

// Rayons du cadre MG : filet extérieur épais, filet intérieur de sertissage.
#define OCT_R_OUT 76.0f
#define OCT_R_MID 68.5f

// Épaisseur monumentale des deux lettres.
#define LETTER_WIDTH 8.5f

// Paramètres 3D du bootscreen (logo 3D + roadster MGF 3D).
#define BOOT_3D_FOV         380.0f
#define BOOT_LOGO_HALF_Z    10.5f
#define BOOT_LOGO_PITCH_DEG 13.0f
#define BOOT_LOGO_REST_YAW  (-24.0f)
#define BOOT_CAR_PITCH_DEG  (-14.0f)
#define BOOT_CAR_REST_YAW   (+32.0f)

struct boot_screen {
    lv_obj_t *root;
    lv_obj_t *canvas;
    lv_timer_t *timer;
    uint32_t started_ms;
    uint32_t elapsed_ms;
    boot_screen_mode_t mode;
};

// ── 1. VERSION 2D MINIMALISTE (CONSERVÉE DE CÔTÉ INTACTE) ───────────────────

static void draw_octagon_2d(lv_layer_t *layer, const ui_layout_t *layout,
                            float cx, float cy, float r, float width,
                            lv_color_t color) {
    const float s = OCT_SIDE_RATIO * r;
    const float points[8][2] = {
        {-s, -r}, {+s, -r}, {+r, -s}, {+r, +s},
        {+s, +r}, {-s, +r}, {-r, +s}, {-r, -s},
    };

    for (unsigned i = 0; i < 8; i++) {
        const unsigned next = (i + 1u) % 8u;
        amber_draw_line(layer, layout, cx + points[i][0], cy + points[i][1],
                        cx + points[next][0], cy + points[next][1], width,
                        color, true);
    }
}

static void mark_line_2d(lv_layer_t *layer, const ui_layout_t *layout, float cx,
                         float cy, float x1, float y1, float x2, float y2,
                         lv_color_t color) {
    amber_draw_line(layer, layout, cx + x1, cy + y1, cx + x2, cy + y2,
                    LETTER_WIDTH, color, true);
}

static void draw_m_2d(lv_layer_t *layer, const ui_layout_t *layout, float cx,
                      float cy, lv_color_t color) {
    mark_line_2d(layer, layout, cx, cy, -52.0f, +20.0f, -52.0f, -20.0f, color);
    mark_line_2d(layer, layout, cx, cy, -52.0f, +20.0f, -43.0f, +29.0f, color);

    mark_line_2d(layer, layout, cx, cy, -52.0f, -20.0f, -41.0f, -31.0f, color);
    mark_line_2d(layer, layout, cx, cy, -41.0f, -31.0f, -30.5f, -5.0f, color);
    mark_line_2d(layer, layout, cx, cy, -30.5f, -5.0f, -15.5f, -48.0f, color);
    mark_line_2d(layer, layout, cx, cy, -15.5f, -48.0f, -10.0f, -48.0f, color);

    mark_line_2d(layer, layout, cx, cy, -10.0f, -48.0f, -10.0f, +48.0f, color);
    mark_line_2d(layer, layout, cx, cy, -10.0f, +48.0f, -17.5f, +48.0f, color);
}

static void draw_g_2d(lv_layer_t *layer, const ui_layout_t *layout, float cx,
                      float cy, lv_color_t color) {
    mark_line_2d(layer, layout, cx, cy, +10.0f, -48.0f, +24.0f, -48.0f, color);
    mark_line_2d(layer, layout, cx, cy, +24.0f, -48.0f, +52.0f, -20.0f, color);
    mark_line_2d(layer, layout, cx, cy, +52.0f, -20.0f, +52.0f, -14.0f, color);

    mark_line_2d(layer, layout, cx, cy, +10.0f, -48.0f, +10.0f, +48.0f, color);
    mark_line_2d(layer, layout, cx, cy, +10.0f, +48.0f, +24.0f, +48.0f, color);
    mark_line_2d(layer, layout, cx, cy, +24.0f, +48.0f, +52.0f, +20.0f, color);

    mark_line_2d(layer, layout, cx, cy, +52.0f, +20.0f, +52.0f, +3.0f, color);
    mark_line_2d(layer, layout, cx, cy, +52.0f, +3.0f, +28.0f, +3.0f, color);
}

static void draw_mg_logo_2d_minimal(lv_layer_t *layer,
                                    const ui_layout_t *layout) {
    const lv_color_t bright = ui_theme_amber_bright();

    draw_octagon_2d(layer, layout, BOOT_CENTER, BOOT_CENTER, OCT_R_OUT, 5.2f,
                    bright);
    draw_octagon_2d(layer, layout, BOOT_CENTER, BOOT_CENTER, OCT_R_MID, 1.3f,
                    ui_theme_amber_separator());

    draw_m_2d(layer, layout, BOOT_CENTER, BOOT_CENTER, bright);
    draw_g_2d(layer, layout, BOOT_CENTER, BOOT_CENTER, bright);
}

// ── 2. MOTEUR DE PROJECTION 2.5D / 3D HOLO-PIXEL ────────────────────────────

typedef struct {
    float cx;
    float cy;
    float cos_yaw;
    float sin_yaw;
    float cos_pitch;
    float sin_pitch;
} boot_proj_t;

typedef struct {
    float x;
    float y;
    float k;
} boot_pt_t;

static boot_proj_t make_proj(float cx, float cy, float yaw_deg,
                             float pitch_deg) {
    const float yaw = yaw_deg * (float)M_PI / 180.0f;
    const float pitch = pitch_deg * (float)M_PI / 180.0f;
    return (boot_proj_t){
        .cx = cx,
        .cy = cy,
        .cos_yaw = cosf(yaw),
        .sin_yaw = sinf(yaw),
        .cos_pitch = cosf(pitch),
        .sin_pitch = sinf(pitch),
    };
}

static boot_pt_t project_3d(const boot_proj_t *proj, float u, float v,
                            float z) {
    const float x1 = u * proj->cos_yaw + z * proj->sin_yaw;
    const float z1 = -u * proj->sin_yaw + z * proj->cos_yaw;
    const float y2 = v * proj->cos_pitch - z1 * proj->sin_pitch;
    const float z2 = v * proj->sin_pitch + z1 * proj->cos_pitch;
    const float k = BOOT_3D_FOV / (BOOT_3D_FOV - z2);
    return (boot_pt_t){
        .x = proj->cx + x1 * k,
        .y = proj->cy + y2 * k,
        .k = k,
    };
}

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

static void draw_pixel_3d(lv_layer_t *layer, const ui_layout_t *layout,
                          const boot_proj_t *proj, float u, float v, float z,
                          float size, lv_color_t color) {
    const boot_pt_t p = project_3d(proj, u, v, z);
    draw_pixel_block(layer, layout, p.x, p.y, size * p.k, size * p.k, color);
}

static void draw_line_3d(lv_layer_t *layer, const ui_layout_t *layout,
                         const boot_proj_t *proj, float u1, float v1, float z1,
                         float u2, float v2, float z2, float width,
                         lv_color_t color, bool rounded) {
    const boot_pt_t a = project_3d(proj, u1, v1, z1);
    const boot_pt_t b = project_3d(proj, u2, v2, z2);
    const float w = width * (a.k + b.k) * 0.5f;
    amber_draw_line(layer, layout, a.x, a.y, b.x, b.y, w, color, rounded);
}

static void draw_scanner_podium(lv_layer_t *layer, const ui_layout_t *layout,
                                float y_ring, float rx, float ry) {
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t sep = ui_theme_amber_separator();

    for (int i = 0; i < 28; i++) {
        const float angle = (float)i * 2.0f * (float)M_PI / 28.0f;
        const lv_color_t c = (i % 7 == 0) ? sep : dim;
        draw_pixel_block(layer, layout, BOOT_CENTER + cosf(angle) * rx,
                         y_ring + sinf(angle) * ry, 3.0f, 2.2f, c);
    }
    // Mirettes de visée CAO à gauche et à droite.
    for (int side = 0; side < 2; side++) {
        const float x = (side == 0) ? 46.0f : 274.0f;
        draw_pixel_block(layer, layout, x, BOOT_CENTER, 1.5f, 9.0f, dim);
        draw_pixel_block(layer, layout, x, BOOT_CENTER, 9.0f, 1.5f, dim);
    }
}

// ── 3. MODE 3D LOGO : EMBLÈME MG OCTOGONAL 3D ROTATIF ───────────────────────

static void draw_octagon_3d(lv_layer_t *layer, const ui_layout_t *layout,
                            const boot_proj_t *proj, float r, float z,
                            float width, lv_color_t color) {
    const float s = OCT_SIDE_RATIO * r;
    const float pts[8][2] = {
        {-s, -r}, {+s, -r}, {+r, -s}, {+r, +s},
        {+s, +r}, {-s, +r}, {-r, +s}, {-r, -s},
    };
    for (int i = 0; i < 8; i++) {
        const int n = (i + 1) & 7;
        draw_line_3d(layer, layout, proj, pts[i][0], pts[i][1], z, pts[n][0],
                     pts[n][1], z, width, color, true);
    }
}

static void draw_mg_letters_3d(lv_layer_t *layer, const ui_layout_t *layout,
                               const boot_proj_t *proj, float z, float width,
                               lv_color_t color) {
    static const float m_segs[][4] = {
        {-52.0f, +20.0f, -52.0f, -20.0f}, {-52.0f, +20.0f, -43.0f, +29.0f},
        {-52.0f, -20.0f, -41.0f, -31.0f}, {-41.0f, -31.0f, -30.5f, -5.0f},
        {-30.5f, -5.0f, -15.5f, -48.0f},  {-15.5f, -48.0f, -10.0f, -48.0f},
        {-10.0f, -48.0f, -10.0f, +48.0f}, {-10.0f, +48.0f, -17.5f, +48.0f},
    };
    static const float g_segs[][4] = {
        {+10.0f, -48.0f, +24.0f, -48.0f}, {+24.0f, -48.0f, +52.0f, -20.0f},
        {+52.0f, -20.0f, +52.0f, -14.0f}, {+10.0f, -48.0f, +10.0f, +48.0f},
        {+10.0f, +48.0f, +24.0f, +48.0f}, {+24.0f, +48.0f, +52.0f, +20.0f},
        {+52.0f, +20.0f, +52.0f, +3.0f},  {+52.0f, +3.0f, +28.0f, +3.0f},
    };

    for (size_t i = 0; i < sizeof(m_segs) / sizeof(m_segs[0]); i++) {
        draw_line_3d(layer, layout, proj, m_segs[i][0], m_segs[i][1], z,
                     m_segs[i][2], m_segs[i][3], z, width, color, true);
    }
    for (size_t i = 0; i < sizeof(g_segs) / sizeof(g_segs[0]); i++) {
        draw_line_3d(layer, layout, proj, g_segs[i][0], g_segs[i][1], z,
                     g_segs[i][2], g_segs[i][3], z, width, color, true);
    }
}

static void draw_mg_logo_3d(lv_layer_t *layer, const ui_layout_t *layout,
                            float yaw_deg) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t sep = ui_theme_amber_separator();

    const boot_proj_t proj =
        make_proj(BOOT_CENTER, 152.0f, yaw_deg, BOOT_LOGO_PITCH_DEG);
    const float zf =
        (proj.cos_yaw >= 0.0f) ? BOOT_LOGO_HALF_Z : -BOOT_LOGO_HALF_Z;
    const float zb = -zf;
    const float s = OCT_SIDE_RATIO * OCT_R_OUT;
    const float pts[8][2] = {
        {-s, -OCT_R_OUT}, {+s, -OCT_R_OUT}, {+OCT_R_OUT, -s}, {+OCT_R_OUT, +s},
        {+s, +OCT_R_OUT}, {-s, +OCT_R_OUT}, {-OCT_R_OUT, +s}, {-OCT_R_OUT, -s},
    };

    draw_scanner_podium(layer, layout, 246.0f, 74.0f, 8.5f);

    // 1. Face arrière (octogone + empreinte des lettres en filigrane sombre).
    draw_octagon_3d(layer, layout, &proj, OCT_R_OUT, zb, 1.4f, dim);
    draw_mg_letters_3d(layer, layout, &proj, zb * 0.55f, 3.2f, dim);

    // 2. Arêtes d'épaisseur 3D entre les 8 sommets avant et arrière.
    for (int i = 0; i < 8; i++) {
        draw_line_3d(layer, layout, &proj, pts[i][0], pts[i][1], zb, pts[i][0],
                     pts[i][1], zf, 1.8f, sep, true);
    }

    // 3. Tramage ASCII / Pixel sur les pans latéraux et supérieurs visibles de
    // la tranche octogonale.
    const float u_side = (proj.sin_yaw >= 0.0f) ? -OCT_R_OUT : OCT_R_OUT;
    const float u_diag = (proj.sin_yaw >= 0.0f) ? -(OCT_R_OUT + s) * 0.5f
                                                : (OCT_R_OUT + s) * 0.5f;
    for (int row = -2; row <= 2; row++) {
        const float v = (float)row * 10.5f;
        for (int d = -1; d <= 1; d++) {
            const float z = (float)d * 4.8f;
            const lv_color_t c = (((row + d) & 1) == 0) ? sep : dim;
            draw_pixel_3d(layer, layout, &proj, u_side, v, z, 2.5f, c);
        }
    }
    // Pan coupé supérieur visible + pan horizontal haut.
    for (int k = 0; k < 5; k++) {
        const float t = -s + (float)k * (2.0f * s / 4.0f);
        draw_pixel_3d(layer, layout, &proj, t, -OCT_R_OUT, 0.0f, 2.5f, dim);
    }
    for (int d = -1; d <= 1; d++) {
        draw_pixel_3d(layer, layout, &proj, u_diag, -(OCT_R_OUT + s) * 0.5f,
                      (float)d * 4.8f, 2.5f, sep);
        draw_pixel_3d(layer, layout, &proj, u_diag, +(OCT_R_OUT + s) * 0.5f,
                      (float)d * 4.8f, 2.5f, dim);
    }

    // 4. Face avant : filet de sertissage, octogone massif et lettres M G en
    // léger haut-relief (zf * 1.15).
    draw_octagon_3d(layer, layout, &proj, OCT_R_MID, zf * 0.92f, 1.3f, sep);
    draw_octagon_3d(layer, layout, &proj, OCT_R_OUT, zf, 4.8f, bright);
    draw_mg_letters_3d(layer, layout, &proj, zf * 1.12f, 7.6f, bright);
}

// ── 4. MODE 3D CAR : SILHOUETTE ROADSTER MGF 3D (COURBES + PIXEL) ───────────
//
// Carrosserie galbée de la MGF (1995) :
//   u ∈ [-94, +92] : axe longitudinal (-94 = nez, +92 = poupe)
//   v ∈ [-30, +29] : axe vertical (-30 = haut pare-brise, +29 = sol)
//   z ∈ [-38, +38] : largeur transversale (galbe en bouteille de Coca,
//                    proue et poupe pincées en plan, ailes bombées).
//
// Les deux courbes directrices du flanc :
// 1) `k_mgf_shoulder` : ligne d'épaule et de profil extérieur continu (du
//    spoiler avant jusqu'au bouclier arrière, en passant par l'aile avant
//    bombée, la ceinture de porte plongeante, la hanche au-dessus de l'écope
//    et le galbe de coffre).
// 2) `k_mgf_centerline` : profil central longitudinal (z = 0) pour donner le
//    vrai bombé 3D du capot avant, de la baie moteur centrale et du coffre.

typedef struct {
    float u;
    float v;
    float hz; // demi-largeur z du flanc à cet endroit
} mgf_sec_t;

static const mgf_sec_t k_mgf_shoulder[] = {
    {-84.0f, +14.0f, 19.0f}, // 0  : lèvre inférieure spoiler avant
    {-91.0f, +10.0f, 21.0f}, // 1  : bas pare-chocs avant arrondi
    {-94.0f, +4.0f, 22.0f},  // 2  : pointe du nez / calandre ovale
    {-91.0f, -1.5f, 25.0f},  // 3  : amorce galbe capot / phares
    {-82.0f, -6.0f, 30.0f},  // 4  : galbe phare avant
    {-68.0f, -10.5f, 35.0f}, // 5  : montée d'aile avant
    {-54.0f, -12.5f, 37.5f}, // 6  : sommet galbé de l'aile avant
    {-38.0f, -11.8f, 36.0f}, // 7  : pied de pare-brise / capot
    {-20.0f, -10.5f, 35.5f}, // 8  : creux de porte (ceinture de caisse)
    {+4.0f, -11.0f, 36.0f},  // 9  : milieu de porte remontant
    {+20.0f, -13.5f, 37.5f}, // 10 : amorce de la hanche arrière
    {+38.0f, -16.8f, 38.5f}, // 11 : galbe de hanche au-dessus de l'écope
    {+56.0f, -18.0f, 38.0f}, // 12 : sommet de l'aile arrière
    {+74.0f, -16.5f, 35.5f}, // 13 : descente du coffre vers le becquet
    {+86.0f, -13.5f, 31.0f}, // 14 : lèvre du becquet arrière intégré
    {+91.0f, -6.0f, 29.0f},  // 15 : galbe des feux arrière
    {+92.0f, +4.0f, 28.0f},  // 16 : pare-chocs arrière bombé
    {+87.0f, +12.0f, 25.0f}, // 17 : jupe inférieure arrière
    {+76.0f, +14.0f, 28.0f}, // 18 : retour derrière la roue arrière
};
#define MGF_SHOULDER_COUNT (sizeof(k_mgf_shoulder) / sizeof(k_mgf_shoulder[0]))

// Arche de passage de roue semi-circulaire (du bord avant au bord arrière).
static void draw_wheel_arch_3d(lv_layer_t *layer, const ui_layout_t *layout,
                               const boot_proj_t *proj, float cu, float cv,
                               float cz, float r_arch, float width,
                               lv_color_t color) {
    const int segs = 12;
    const float step = (float)M_PI / (float)segs;
    boot_pt_t prev = project_3d(proj, cu - r_arch, cv, cz);
    for (int i = 1; i <= segs; i++) {
        const float a = (float)M_PI + (float)i * step;
        const boot_pt_t cur = project_3d(proj, cu + r_arch * cosf(a),
                                         cv + r_arch * sinf(a) * 0.92f, cz);
        amber_draw_line(layer, layout, prev.x, prev.y, cur.x, cur.y, width,
                        color, true);
        prev = cur;
    }
}

// Roue 3D avec vraie largeur de pneu (disque externe + disque interne reliés
// par la bande de roulement) et jante 5 branches MGF.
static void draw_wheel_3d(lv_layer_t *layer, const ui_layout_t *layout,
                          const boot_proj_t *proj, float cu, float cv,
                          float z_outer, float z_inner, float r_tire,
                          float r_rim, float spin_deg, lv_color_t tire_col,
                          lv_color_t rim_col, lv_color_t inner_col) {
    const int segs = 18;
    const float step = 2.0f * (float)M_PI / (float)segs;

    // 1. Flanc interne du pneu + épaulements de bande de roulement.
    boot_pt_t prev_in = project_3d(proj, cu + r_tire, cv, z_inner);
    boot_pt_t prev_out = project_3d(proj, cu + r_tire, cv, z_outer);
    for (int i = 1; i <= segs; i++) {
        const float a = (float)i * step;
        const float ca = cosf(a);
        const float sa = sinf(a);
        const boot_pt_t cur_in =
            project_3d(proj, cu + r_tire * ca, cv + r_tire * sa, z_inner);
        const boot_pt_t cur_out =
            project_3d(proj, cu + r_tire * ca, cv + r_tire * sa, z_outer);
        amber_draw_line(layer, layout, prev_in.x, prev_in.y, cur_in.x, cur_in.y,
                        1.1f, inner_col, true);
        if ((i % 3) == 0) {
            amber_draw_line(layer, layout, cur_in.x, cur_in.y, cur_out.x,
                            cur_out.y, 1.1f, inner_col, false);
        }
        prev_in = cur_in;
        prev_out = cur_out;
    }

    // 2. Flanc externe du pneu + cercle de jante alliage.
    prev_out = project_3d(proj, cu + r_tire, cv, z_outer);
    boot_pt_t prev_rim = project_3d(proj, cu + r_rim, cv, z_outer);
    for (int i = 1; i <= segs; i++) {
        const float a = (float)i * step;
        const float ca = cosf(a);
        const float sa = sinf(a);
        const boot_pt_t cur_out =
            project_3d(proj, cu + r_tire * ca, cv + r_tire * sa, z_outer);
        const boot_pt_t cur_rim =
            project_3d(proj, cu + r_rim * ca, cv + r_rim * sa, z_outer);
        amber_draw_line(layer, layout, prev_out.x, prev_out.y, cur_out.x,
                        cur_out.y, 2.0f, tire_col, true);
        amber_draw_line(layer, layout, prev_rim.x, prev_rim.y, cur_rim.x,
                        cur_rim.y, 1.1f, rim_col, true);
        prev_out = cur_out;
        prev_rim = cur_rim;
    }

    // 3. Jante 5 branches qui tournent + moyeu central pixelisé.
    for (int s = 0; s < 5; s++) {
        const float a = (spin_deg + (float)(s * 72)) * (float)M_PI / 180.0f;
        draw_line_3d(layer, layout, proj, cu, cv, z_outer, cu + r_rim * cosf(a),
                     cv + r_rim * sinf(a), z_outer, 1.4f, rim_col, true);
        draw_pixel_3d(layer, layout, proj, cu + (r_rim - 1.5f) * cosf(a),
                      cv + (r_rim - 1.5f) * sinf(a), z_outer, 2.0f, rim_col);
    }
    draw_pixel_3d(layer, layout, proj, cu, cv, z_outer, 2.8f, tire_col);
}

// Trace un arceau courbe (arc transversal + épaisseur longitudinale) pour les
// sièges baquets / roll-hoops et le volant.
static void draw_hoop_3d(lv_layer_t *layer, const ui_layout_t *layout,
                         const boot_proj_t *proj, float u_base, float u_top,
                         float v_base, float v_top, float z_center,
                         float half_w, lv_color_t color) {
    const int segs = 8;
    for (int i = 0; i < segs; i++) {
        const float a0 = (float)M_PI * (float)i / (float)segs;
        const float a1 = (float)M_PI * (float)(i + 1) / (float)segs;
        const float s0 = sinf(a0);
        const float s1 = sinf(a1);
        const float u0 = u_base + (u_top - u_base) * s0;
        const float u1 = u_base + (u_top - u_base) * s1;
        const float v0 = v_base + (v_top - v_base) * s0;
        const float v1 = v_base + (v_top - v_base) * s1;
        const float z0 = z_center - half_w * cosf(a0);
        const float z1 = z_center - half_w * cosf(a1);
        draw_line_3d(layer, layout, proj, u0, v0, z0, u1, v1, z1, 1.5f, color,
                     true);
    }
}

static void draw_mgf_flank(lv_layer_t *layer, const ui_layout_t *layout,
                           const boot_proj_t *proj, float side_sign,
                           float spin_deg, lv_color_t line_col,
                           lv_color_t detail_col, lv_color_t inner_col,
                           float width) {
    // 1. Ligne d'épaule galbée continue (du spoiler avant au bouclier arrière).
    for (size_t i = 0; i + 1 < MGF_SHOULDER_COUNT; i++) {
        const float z1 = side_sign * k_mgf_shoulder[i].hz;
        const float z2 = side_sign * k_mgf_shoulder[i + 1].hz;
        draw_line_3d(layer, layout, proj, k_mgf_shoulder[i].u,
                     k_mgf_shoulder[i].v, z1, k_mgf_shoulder[i + 1].u,
                     k_mgf_shoulder[i + 1].v, z2, width, line_col, true);
    }

    // 2. Montant de pare-brise incliné (A-pillar) + vitre latérale + rétro.
    draw_line_3d(layer, layout, proj, -38.0f, -11.8f, side_sign * 34.5f, -24.0f,
                 -21.5f, side_sign * 31.5f, width, line_col, true);
    draw_line_3d(layer, layout, proj, -24.0f, -21.5f, side_sign * 31.5f, -12.0f,
                 -28.5f, side_sign * 28.0f, width, line_col, true);
    // Retour du montant et déflecteur de vitre.
    draw_line_3d(layer, layout, proj, -12.0f, -28.5f, side_sign * 28.0f, -9.0f,
                 -27.0f, side_sign * 28.5f, 1.2f, detail_col, true);
    draw_line_3d(layer, layout, proj, -9.0f, -27.0f, side_sign * 28.5f, -16.0f,
                 -10.8f, side_sign * 35.0f, 1.1f, detail_col, true);
    // Rétroviseur profilé MGF sur la porte.
    draw_pixel_3d(layer, layout, proj, -27.0f, -14.5f, side_sign * 39.5f, 3.0f,
                  line_col);
    draw_line_3d(layer, layout, proj, -29.0f, -12.5f, side_sign * 35.5f, -27.0f,
                 -14.5f, side_sign * 39.5f, 1.3f, line_col, true);

    // 3. Bas de caisse profilé et passages de roues (avant u=-55, arrière u=+55).
    const float z_sill = side_sign * 35.5f;
    draw_line_3d(layer, layout, proj, -84.0f, +14.0f, side_sign * 19.0f, -73.0f,
                 +14.0f, side_sign * 34.5f, width, line_col, true);
    draw_wheel_arch_3d(layer, layout, proj, -55.0f, +14.0f, side_sign * 36.5f,
                       18.0f, width * 0.9f, detail_col);
    // Bas de caisse pincé entre les deux roues + nervure inférieure de porte.
    draw_line_3d(layer, layout, proj, -37.0f, +14.0f, z_sill, +37.0f, +14.0f,
                 z_sill, width, line_col, true);
    draw_line_3d(layer, layout, proj, -32.0f, +8.5f, side_sign * 36.5f, +32.0f,
                 +7.0f, side_sign * 37.0f, 1.0f, inner_col, true);
    draw_wheel_arch_3d(layer, layout, proj, +55.0f, +14.0f, side_sign * 37.0f,
                       18.5f, width * 0.9f, detail_col);
    draw_line_3d(layer, layout, proj, +73.5f, +14.0f, side_sign * 35.0f, +76.0f,
                 +14.0f, side_sign * 28.0f, width, line_col, true);

    // 4. Écope d'air latérale ovalisée (signature MGF derrière la porte) +
    //    creux en profondeur vers le moteur central.
    const float z_scoop = side_sign * 37.2f;
    const float z_scoop_in = side_sign * 32.5f;
    draw_line_3d(layer, layout, proj, +10.0f, -4.5f, z_scoop, +26.0f, -7.0f,
                 z_scoop, 1.3f, detail_col, true);
    draw_line_3d(layer, layout, proj, +26.0f, -7.0f, z_scoop, +31.0f, -3.5f,
                 z_scoop, 1.3f, detail_col, true);
    draw_line_3d(layer, layout, proj, +31.0f, -3.5f, z_scoop, +27.0f, +3.5f,
                 z_scoop, 1.3f, detail_col, true);
    draw_line_3d(layer, layout, proj, +27.0f, +3.5f, z_scoop, +14.0f, +2.0f,
                 z_scoop, 1.3f, detail_col, true);
    draw_line_3d(layer, layout, proj, +14.0f, +2.0f, z_scoop, +10.0f, -4.5f,
                 z_scoop, 1.3f, detail_col, true);
    // Ailettes internes pixelisées en retrait dans l'écope.
    for (int k = 0; k < 3; k++) {
        draw_pixel_3d(layer, layout, proj, +16.5f + (float)k * 4.2f,
                      -2.5f + (float)k * 0.7f, z_scoop_in, 2.3f, detail_col);
    }

    // 5. Roues 3D cylindriques (avant u=-55, arrière u=+55).
    draw_wheel_3d(layer, layout, proj, -55.0f, +14.0f, side_sign * 36.2f,
                  side_sign * 25.0f, 15.0f, 9.8f, spin_deg, line_col,
                  detail_col, inner_col);
    draw_wheel_3d(layer, layout, proj, +55.0f, +14.0f, side_sign * 36.8f,
                  side_sign * 24.5f, 15.6f, 10.2f, spin_deg, line_col,
                  detail_col, inner_col);
}

// Trace un phare avant MGF : ovale bien rond tirant légèrement sur l'amande
// côté calandre, posé sur le galbe supérieur de l'aile avant, avec le petit
// clignotant ovale juste en dessous dans le pare-chocs.
static void draw_headlight_3d(lv_layer_t *layer, const ui_layout_t *layout,
                              const boot_proj_t *proj, float side_sign,
                              lv_color_t rim_col, lv_color_t bulb_col) {
    const int segs = 16;
    const float step = 2.0f * (float)M_PI / (float)segs;
    // Centre de l'optique sur le galbe avant-latéral du capot.
    const float cu = -83.0f;
    const float cv = -3.8f;
    const float cz = side_sign * 21.5f;

    boot_pt_t prev = {0};
    for (int i = 0; i <= segs; i++) {
        const float a = (float)i * step;
        const float ca = cosf(a); // -1 = côté calandre, +1 = côté aile externe
        const float sa = sinf(a); // -1 = haut capot, +1 = bas pare-chocs
        // Ovale bien rond (hauteur généreuse en v), à peine pincé côté
        // calandre pour l'esprit "rond tirant sur l'amande".
        const float pinch = 1.0f + 0.12f * ca;
        const float u = cu + 5.2f * ca + 1.8f * sa;
        const float v = cv + 4.4f * sa * pinch - 0.6f * ca;
        const float z = cz + side_sign * (5.2f * ca - 0.8f * sa);
        const boot_pt_t cur = project_3d(proj, u, v, z);
        if (i > 0) {
            amber_draw_line(layer, layout, prev.x, prev.y, cur.x, cur.y, 1.7f,
                            rim_col, true);
        }
        prev = cur;
    }

    // Double réflecteur rond interne + clignotant ovale sous le phare.
    draw_pixel_3d(layer, layout, proj, cu - 1.2f, cv + 0.2f,
                  cz - side_sign * 1.3f, 2.5f, bulb_col);
    draw_pixel_3d(layer, layout, proj, cu + 1.4f, cv - 0.3f,
                  cz + side_sign * 1.6f, 2.8f, bulb_col);
    // Clignotant orange logé dans le pare-chocs juste sous le phare.
    draw_line_3d(layer, layout, proj, -86.5f, +2.8f, side_sign * 20.5f, -82.0f,
                 +2.4f, side_sign * 24.5f, 2.2f, bulb_col, true);
}

// Trace un arc transversal bombé (de -hz à +hz, avec une flèche verticale dv
// et longitudinale du au centre z=0) pour donner un galbe rond au capot, au
// pare-brise, au pare-chocs et au coffre.
static void draw_curved_rib_3d(lv_layer_t *layer, const ui_layout_t *layout,
                               const boot_proj_t *proj, float u_side,
                               float v_side, float hz, float du_center,
                               float dv_center, float width, lv_color_t color) {
    const int segs = 8;
    for (int i = 0; i < segs; i++) {
        const float t0 = -1.0f + 2.0f * (float)i / (float)segs;
        const float t1 = -1.0f + 2.0f * (float)(i + 1) / (float)segs;
        const float b0 = 1.0f - t0 * t0; // parabole : 1 au centre, 0 aux bords
        const float b1 = 1.0f - t1 * t1;
        draw_line_3d(layer, layout, proj, u_side + du_center * b0,
                     v_side + dv_center * b0, t0 * hz, u_side + du_center * b1,
                     v_side + dv_center * b1, t1 * hz, width, color, true);
    }
}

static void draw_mgf_car_3d(lv_layer_t *layer, const ui_layout_t *layout,
                            float yaw_deg, float spin_deg) {
    const lv_color_t bright = ui_theme_amber_bright();
    const lv_color_t dim = ui_theme_amber_dim();
    const lv_color_t sep = ui_theme_amber_separator();

    const boot_proj_t proj =
        make_proj(BOOT_CENTER, 152.0f, yaw_deg, BOOT_CAR_PITCH_DEG);
    const float near_sign = (proj.cos_yaw >= 0.0f) ? +1.0f : -1.0f;
    const float far_sign = -near_sign;

    draw_scanner_podium(layer, layout, 246.0f, 96.0f, 10.0f);

    // 1. Flanc arrière-plan en filigrane sombre (dim) : carrosserie seule
    //    (les roues arrière-plan ne sont pas tracées à travers les phares).
    for (size_t i = 0; i + 1 < MGF_SHOULDER_COUNT; i++) {
        const float z1 = far_sign * k_mgf_shoulder[i].hz;
        const float z2 = far_sign * k_mgf_shoulder[i + 1].hz;
        draw_line_3d(layer, layout, &proj, k_mgf_shoulder[i].u,
                     k_mgf_shoulder[i].v, z1, k_mgf_shoulder[i + 1].u,
                     k_mgf_shoulder[i + 1].v, z2, 1.0f, dim, true);
    }
    draw_line_3d(layer, layout, &proj, -38.0f, -11.8f, far_sign * 34.5f, -24.0f,
                 -21.5f, far_sign * 31.5f, 1.0f, dim, true);
    draw_line_3d(layer, layout, &proj, -24.0f, -21.5f, far_sign * 31.5f, -12.0f,
                 -28.5f, far_sign * 28.0f, 1.0f, dim, true);

    // 2. Couples transversaux BOMBÉS (arches paraboliques 3D) : plus aucune
    //    traverse plate, toute la carrosserie est ronde en coupe transversale !
    //    - Spoiler & prise d'air basse avant :
    draw_curved_rib_3d(layer, layout, &proj, -84.0f, +14.0f, 19.0f, -4.0f,
                       +0.5f, 1.2f, sep);
    //    - Nez / bouclier avant arrondi en plan :
    draw_curved_rib_3d(layer, layout, &proj, -94.0f, +4.0f, 22.0f, -4.5f, +0.5f,
                       1.6f, bright);
    //    - Milieu du capot avant bombé :
    draw_curved_rib_3d(layer, layout, &proj, -54.0f, -12.5f, 37.5f, -2.0f,
                       -3.0f, 1.1f, dim);
    //    - Base du pare-brise incurvée :
    draw_curved_rib_3d(layer, layout, &proj, -38.0f, -11.8f, 34.5f, -5.0f,
                       -2.5f, 1.4f, sep);
    //    - Traverse supérieure du pare-brise panoramique (cintrée) :
    draw_curved_rib_3d(layer, layout, &proj, -12.0f, -28.5f, 28.0f, -3.5f,
                       -2.2f, 1.8f, bright);
    //    - Couvre-tonneau derrière l'habitacle :
    draw_curved_rib_3d(layer, layout, &proj, +20.0f, -13.5f, 36.0f, -2.0f,
                       -1.5f, 1.3f, sep);
    //    - Capot moteur central bombé :
    draw_curved_rib_3d(layer, layout, &proj, +56.0f, -18.0f, 38.0f, +1.0f,
                       -2.2f, 1.2f, sep);
    //    - Becquet de coffre arrière galbé :
    draw_curved_rib_3d(layer, layout, &proj, +86.0f, -13.5f, 31.0f, +3.5f,
                       -1.5f, 1.6f, bright);
    //    - Bandeau des feux arrière et pare-chocs bombé :
    draw_curved_rib_3d(layer, layout, &proj, +92.0f, +4.0f, 28.0f, +4.0f, +0.0f,
                       1.4f, sep);
    draw_curved_rib_3d(layer, layout, &proj, +87.0f, +12.0f, 25.0f, +3.0f,
                       +0.5f, 1.1f, dim);

    // 3. Épine dorsale et nervures courbes du capot plongeant MGF.
    static const float hood_spine[][2] = {
        {-98.5f, +4.5f},  {-86.0f, -8.2f},  {-70.0f, -13.2f},
        {-54.0f, -15.5f}, {-43.0f, -14.3f},
    };
    for (size_t i = 0; i + 1 < sizeof(hood_spine) / sizeof(hood_spine[0]);
         i++) {
        draw_line_3d(layer, layout, &proj, hood_spine[i][0], hood_spine[i][1],
                     0.0f, hood_spine[i + 1][0], hood_spine[i + 1][1], 0.0f,
                     1.1f, dim, true);
        draw_line_3d(layer, layout, &proj, hood_spine[i][0] + 1.5f,
                     hood_spine[i][1] + 0.8f, -14.0f,
                     hood_spine[i + 1][0] + 1.5f, hood_spine[i + 1][1] + 0.8f,
                     -17.0f, 1.1f, sep, true);
        draw_line_3d(layer, layout, &proj, hood_spine[i][0] + 1.5f,
                     hood_spine[i][1] + 0.8f, +14.0f,
                     hood_spine[i + 1][0] + 1.5f, hood_spine[i + 1][1] + 0.8f,
                     +17.0f, 1.1f, sep, true);
    }

    // 4. Calandre ovale MGF (divisée par le montant central et l'écusson MG),
    //    bouche inférieure et phares ronds-amande + clignotants.
    draw_curved_rib_3d(layer, layout, &proj, -94.0f, +1.2f, 12.5f, -4.2f, +0.2f,
                       1.3f, sep);
    draw_curved_rib_3d(layer, layout, &proj, -92.0f, +8.5f, 15.0f, -4.0f, +1.5f,
                       1.2f, dim);
    draw_pixel_3d(layer, layout, &proj, -98.2f, +2.6f, 0.0f, 3.2f, bright);
    // Phare arrière-plan en ton atténué, phare premier plan en ambre vif.
    draw_headlight_3d(layer, layout, &proj, far_sign, dim, sep);
    draw_headlight_3d(layer, layout, &proj, near_sign, bright, bright);
    for (int side = -1; side <= 1; side += 2) {
        const float s = (float)side;
        // Feux arrière galbés + double sortie d'échappement.
        draw_pixel_3d(layer, layout, &proj, +92.5f, -5.0f, s * 23.0f, 3.0f,
                      bright);
        draw_pixel_3d(layer, layout, &proj, +92.0f, -2.0f, s * 23.0f, 2.5f,
                      sep);
        draw_pixel_3d(layer, layout, &proj, +89.5f, +12.5f, s * 16.5f, 2.6f,
                      bright);
    }

    // 5. Habitacle cabriolet : arceaux arrondis des sièges baquets + volant.
    for (int side = -1; side <= 1; side += 2) {
        const float zc = (float)side * 15.5f;
        draw_hoop_3d(layer, layout, &proj, +14.0f, +17.0f, -12.0f, -24.5f, zc,
                     6.5f, bright);
        draw_hoop_3d(layer, layout, &proj, +11.0f, +14.0f, -11.5f, -22.5f, zc,
                     5.0f, sep);
    }
    // Volant à droite (RHD britannique !) ou gauche : anneau incliné dans le
    // cockpit.
    draw_hoop_3d(layer, layout, &proj, -22.0f, -19.0f, -11.0f, -18.5f, +15.0f,
                 4.8f, sep);

    // 6. Grille d'aération du capot moteur central en blocs pixels ASCII 3D
    //    (suit le bombé transversal du capot arrière).
    for (int row = 0; row < 3; row++) {
        const float u = +38.0f + (float)row * 10.0f;
        for (int col = -2; col <= 2; col++) {
            const float z = (float)col * 7.5f;
            const float t = z / 36.0f;
            const float v = -16.2f - 2.0f * (1.0f - t * t);
            draw_pixel_3d(layer, layout, &proj, u, v, z, 2.2f,
                          (col == 0) ? sep : dim);
        }
    }

    // 7. Flanc premier plan en ambre vif (bright / separator).
    draw_mgf_flank(layer, layout, &proj, near_sign, spin_deg, bright, sep, dim,
                   2.0f);
}

// ── 5. RENDU & SÉQUENCE D'ANIMATION ─────────────────────────────────────────

static void boot_canvas_draw_cb(lv_event_t *event) {
    if (lv_event_get_code(event) != LV_EVENT_DRAW_MAIN) return;

    lv_layer_t *layer = lv_event_get_layer(event);
    lv_obj_t *canvas = lv_event_get_target(event);
    boot_screen_t *screen = lv_obj_get_user_data(canvas);
    if (layer == NULL || canvas == NULL || screen == NULL) return;

    lv_area_t area;
    lv_obj_get_coords(canvas, &area);
    const ui_layout_t layout = amber_draw_layout(&area);

    if (screen->mode == BOOT_SCREEN_MODE_2D_MINIMAL) {
        draw_mg_logo_2d_minimal(layer, &layout);
        return;
    }

    if (screen->mode == BOOT_SCREEN_MODE_3D_CAR) {
        const uint32_t car_ms =
            screen->elapsed_ms > BOOT_SCREEN_INTRO_Logo_MS
                ? (screen->elapsed_ms - BOOT_SCREEN_INTRO_Logo_MS)
                : 0U;
        const float yaw_deg = BOOT_CAR_REST_YAW + (float)car_ms * 0.045f;
        const float spin_deg = (float)car_ms * 0.25f;
        draw_mgf_car_3d(layer, &layout, yaw_deg, spin_deg);
        return;
    }

    // BOOT_SCREEN_MODE_3D_LOGO (défaut) : rotation fluide du logo MG 3D.
    const float yaw_deg =
        BOOT_LOGO_REST_YAW + (float)screen->elapsed_ms * 0.055f;
    draw_mg_logo_3d(layer, &layout, yaw_deg);
}

static void boot_intro_timer_cb(lv_timer_t *timer) {
    boot_screen_t *screen = lv_timer_get_user_data(timer);
    if (screen == NULL) return;

    const uint32_t elapsed = lv_tick_get() - screen->started_ms;
    if (elapsed >= BOOT_SCREEN_INTRO_TOTAL_MS) {
        boot_screen_destroy(screen);
        return;
    }
    boot_screen_set_elapsed(screen, elapsed);
}

boot_screen_t *boot_screen_create(lv_obj_t *parent) {
    if (parent == NULL) return NULL;

    boot_screen_t *screen = lv_malloc(sizeof(*screen));
    if (screen == NULL) return NULL;
    lv_memzero(screen, sizeof(*screen));
    screen->mode = BOOT_SCREEN_MODE_3D_LOGO;

    screen->root = amber_ui_root_create(parent);
    if (screen->root == NULL) goto fail;

    screen->canvas =
        amber_ui_canvas_create(screen->root, screen, boot_canvas_draw_cb);
    if (screen->canvas == NULL) goto fail;

    return screen;

fail:
    boot_screen_destroy(screen);
    return NULL;
}

void boot_screen_set_mode(boot_screen_t *screen, boot_screen_mode_t mode) {
    if (screen == NULL || screen->mode == mode) return;
    screen->mode = mode;
    if (screen->canvas != NULL) lv_obj_invalidate(screen->canvas);
}

void boot_screen_set_elapsed(boot_screen_t *screen, uint32_t elapsed_ms) {
    if (screen == NULL) return;
    screen->elapsed_ms = elapsed_ms;
    if (screen->mode != BOOT_SCREEN_MODE_2D_MINIMAL) {
        screen->mode = (elapsed_ms < BOOT_SCREEN_INTRO_Logo_MS)
                           ? BOOT_SCREEN_MODE_3D_LOGO
                           : BOOT_SCREEN_MODE_3D_CAR;
    }
    if (screen->canvas != NULL) lv_obj_invalidate(screen->canvas);
}

void boot_screen_start_intro(boot_screen_t *screen) {
    if (screen == NULL || screen->root == NULL) return;
    lv_obj_move_foreground(screen->root);
    screen->started_ms = lv_tick_get();
    screen->elapsed_ms = 0U;
    screen->mode = BOOT_SCREEN_MODE_3D_LOGO;
    if (screen->timer == NULL) {
        screen->timer = lv_timer_create(boot_intro_timer_cb,
                                        BOOT_SCREEN_TICK_PERIOD_MS, screen);
    }
    if (screen->canvas != NULL) lv_obj_invalidate(screen->canvas);
}

void boot_screen_destroy(boot_screen_t *screen) {
    if (screen == NULL) return;
    if (screen->timer != NULL) {
        lv_timer_delete(screen->timer);
        screen->timer = NULL;
    }
    if (screen->root != NULL) lv_obj_delete(screen->root);
    lv_free(screen);
}
