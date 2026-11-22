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

// Paramètres 3D du bootscreen (logo MG 3D rotatif).
#define BOOT_3D_FOV         380.0f
#define BOOT_LOGO_HALF_Z    10.5f
#define BOOT_LOGO_PITCH_DEG 13.0f
#define BOOT_LOGO_REST_YAW  (-24.0f)

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
