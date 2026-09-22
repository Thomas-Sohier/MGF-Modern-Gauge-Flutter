// Harnais de simulation hôte : compile la même UI LVGL que la cible ESP32-S3,
// rend une frame offscreen avec des données ECU mock et exporte le rendu en PNG
// (golden test).
//
// Usage : gen_golden <chemin_png>

#include "lvgl.h"
#include "ui/screens/boot_screen.h"
#include "ui/screens/style_amber.h"
#include "ui/screens/clock_screen.h"
#include "ui/screens/music_screen.h"
#include "ui/screens/navigation_screen.h"
#include "ui/screens/faults_screen.h"
#include "ui/screens/temps_screen.h"
#include "ui/screens/injection_screen.h"
#include "ui/screens/lambda_screen.h"
#include "ui/screens/ignition_screen.h"
#include "ui/screens/idle_screen.h"
#include "ui/screens/admission_screen.h"
#include "ui/fonts/ui_fonts.h"
#include "domain/ecu_data.h"
#include "ui/ui_layout.h"

#ifndef TTF_PATH
#define TTF_PATH "../main/fonts/Michroma-Regular.ttf"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// Buffer partiel dimensionné pour la plus grande largeur possible.
#define BUF_W 1024

// ── Tick LVGL basé sur l'horloge monotone ────────────────────────────────────
static uint32_t tick_cb(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ── Display offscreen (flush no-op) ──────────────────────────────────────────
static uint8_t draw_buf[BUF_W * 80 * 4];

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    LV_UNUSED(area);
    LV_UNUSED(px_map);
    lv_display_flush_ready(disp);
}

// ── Export ARGB8888 -> PNG RGB via stb ───────────────────────────────────────
// Masque les pixels hors du disque inscrit (panneau physiquement rond
// de la LILYGO T-RGB H597) -> golden représentatif de l'écran réel.
static int write_png(const lv_draw_buf_t *snap, const char *path) {
    const uint32_t w = snap->header.w;
    const uint32_t h = snap->header.h;
    const uint32_t stride = snap->header.stride;
    uint8_t *rgb = malloc((size_t)w * h * 3);
    if (!rgb) return 0;

    const float cx = (w - 1) * 0.5f, cy = (h - 1) * 0.5f;
    const float r = (float)((w < h ? w : h)) * 0.5f;
    const float r2 = r * r;

    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *row = snap->data + (size_t)y * stride;
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *p = row + (size_t)x * 4; // ARGB8888 little-endian : B,G,R,A
            uint8_t *o = rgb + ((size_t)y * w + x) * 3;
            const float dx = (float)x - cx, dy = (float)y - cy;
            if (dx * dx + dy * dy > r2) {   // hors du disque : bezel noir
                o[0] = o[1] = o[2] = 0;
            } else {
                o[0] = p[2]; // R
                o[1] = p[1]; // G
                o[2] = p[0]; // B
            }
        }
    }
    const int ok = stbi_write_png(path, (int)w, (int)h, 3, rgb, (int)w * 3);
    free(rgb);
    return ok;
}

// Charge un fichier entier en mémoire (malloc, non libéré).
static void *load_file(const char *path, size_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = malloc((size_t)n);
    if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
    fclose(f);
    if (buf) *out_size = (size_t)n;
    return buf;
}

int main(int argc, char **argv) {
    // Usage : gen_golden [amber|boot|clock|music|navigation|faults|temps|
    //                     injection|lambda|ignition|idle|admission] png
    const char *style = (argc > 1) ? argv[1] : "amber";
    const char *out = (argc > 2) ? argv[2] : "rpm_amber.png";

    // Cible LILYGO T-RGB H597 : écran IPS ROND 480x480 (driver ST7701S,
    // interface RGB). On rend à la résolution réelle, avec masque circulaire.
    const int32_t W = UI_DISPLAY_SIZE_PX;
    const int32_t H = UI_DISPLAY_SIZE_PX;

    lv_init();
    lv_tick_set_cb(tick_cb);

    // Police Michroma (sinon repli Montserrat).
    size_t ttf_size = 0;
    void *ttf = load_file(TTF_PATH, &ttf_size);
    if (ttf) ui_fonts_init(ttf, ttf_size);
    else fprintf(stderr, "warn: TTF introuvable (%s), repli Montserrat\n", TTF_PATH);

    lv_display_t *disp = lv_display_create(W, H);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    const ecu_data_t mock = {
        .connected = true, .faults_available = true,
        .fault_flags = ECU_FAULT_INTAKE_AIR_SENSOR,
        .rpm = 875, .throttle = 7, .coolant_temp = 89,
        .battery_voltage = 14.2f, .oil_temp = 96, .ambient_temp = 22,
        .intake_air_temp = 31, .fuel_rail_temp = 38, .map_sensor_kpa = 34,
        .throttle_pot_voltage = 0.72f, .ignition_advance = 14.5f,
        .ignition_advance_offset = -1.2f, .coil_1_charge_time = 2.45f,
        .coil_2_charge_time = 2.52f, .coil_time_microseconds = 2480,
        .injector_1_pw = 2.18f, .injector_2_pw = 2.24f,
        .fuelling_feedback_percent = 101, .short_term_trim_percent = 2.8f,
        .long_term_trim = -1.7f, .lambda_mv = 680, .o2_mv = 665,
        .estimated_air_fuel = 14.65f, .lambda_sensor_duty_cycle = 53,
        .idle_setpoint = 850, .idle_adjuster_rpm = 18, .idle_error = 25,
        .idle_valve_position = 34, .idle_base_position = 30,
    };

    if (strcmp(style, "boot") == 0) boot_screen_create(screen);
    else if (strcmp(style, "amber") == 0) amber_screen_update(amber_screen_create(screen), &mock);
    else if (strcmp(style, "clock") == 0) clock_screen_update(clock_screen_create(screen), &mock);
    else if (strcmp(style, "music") == 0) music_screen_update(music_screen_create(screen), &mock);
    else if (strcmp(style, "navigation") == 0) navigation_screen_update(navigation_screen_create(screen), &mock);
    else if (strcmp(style, "faults") == 0) faults_screen_update(faults_screen_create(screen), &mock);
    else if (strcmp(style, "temps") == 0) temps_screen_update(temps_screen_create(screen), &mock);
    else if (strcmp(style, "injection") == 0) injection_screen_update(injection_screen_create(screen), &mock);
    else if (strcmp(style, "lambda") == 0) lambda_screen_update(lambda_screen_create(screen), &mock);
    else if (strcmp(style, "ignition") == 0) ignition_screen_update(ignition_screen_create(screen), &mock);
    else if (strcmp(style, "idle") == 0) idle_screen_update(idle_screen_create(screen), &mock);
    else if (strcmp(style, "admission") == 0) admission_screen_update(admission_screen_create(screen), &mock);
    else {
        fprintf(stderr, "style inconnu : %s\n", style);
        return 2;
    }

    // Résout la disposition puis force un cycle de rendu.
    lv_obj_update_layout(screen);
    lv_refr_now(disp);

    lv_draw_buf_t *snap = lv_snapshot_take(screen, LV_COLOR_FORMAT_ARGB8888);
    if (!snap) {
        fprintf(stderr, "lv_snapshot_take a echoue\n");
        return 1;
    }

    if (!write_png(snap, out)) {
        fprintf(stderr, "ecriture PNG echouee: %s\n", out);
        lv_draw_buf_destroy(snap);
        return 1;
    }

    printf("golden ecrit: %s (%ux%u)\n", out, snap->header.w, snap->header.h);
    lv_draw_buf_destroy(snap);
    return 0;
}
