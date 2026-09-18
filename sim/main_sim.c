// Harnais de simulation hôte : compile la MÊME UI que la cible ESP32-P4
// (dual_arc_dial.c + rpm_screen.c) contre LVGL, rend une frame offscreen avec
// des données ECU mock, et exporte le rendu en PNG (golden test).
//
// Usage : gen_golden <chemin_png>

#include "lvgl.h"
#include "rpm_screen.h"
#include "style_amber.h"
#include "style_cream.h"
#include "ui_fonts.h"
#include "ecu_data.h"

#ifndef TTF_PATH
#define TTF_PATH "../main/fonts/JetBrainsMono-Bold.ttf"
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
static int write_png(const lv_draw_buf_t *snap, const char *path) {
    const uint32_t w = snap->header.w;
    const uint32_t h = snap->header.h;
    const uint32_t stride = snap->header.stride;
    uint8_t *rgb = malloc((size_t)w * h * 3);
    if (!rgb) return 0;

    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *row = snap->data + (size_t)y * stride;
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *p = row + (size_t)x * 4; // ARGB8888 little-endian : B,G,R,A
            uint8_t *o = rgb + ((size_t)y * w + x) * 3;
            o[0] = p[2]; // R
            o[1] = p[1]; // G
            o[2] = p[0]; // B
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
    // Usage : gen_golden [dual|amber|cream] <chemin_png>
    const char *style = (argc > 1) ? argv[1] : "dual";
    const char *out = (argc > 2) ? argv[2] : "rpm_screen.png";

    // L'ambre cible un écran ROND ~52 mm -> canvas carré. Les autres restent
    // en 1024x600 (écran paysage MIPI-DSI) pour l'instant.
    const int is_amber = (strcmp(style, "amber") == 0);
    const int32_t W = is_amber ? 720 : 1024;
    const int32_t H = is_amber ? 720 : 600;

    lv_init();
    lv_tick_set_cb(tick_cb);

    // Polices monospace JetBrains Mono (sinon repli Montserrat).
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

    if (strcmp(style, "amber") == 0) {
        // Ralenti, valeurs de la photo 1.
        const ecu_data_t mock = {.connected = true, .rpm = 800, .throttle = 6,
                                 .coolant_temp = 89, .battery_voltage = 14.2f,
                                 .oil_temp = 91};
        amber_screen_update(amber_screen_create(screen), &mock);
    } else if (strcmp(style, "cream") == 0) {
        // Ralenti, valeurs de la photo 2.
        const ecu_data_t mock = {.connected = true, .rpm = 800, .throttle = 6,
                                 .coolant_temp = 87, .battery_voltage = 14.0f,
                                 .oil_temp = 90};
        cream_screen_update(cream_screen_create(screen), &mock);
    } else {
        // Écran de base : régime en zone de danger (segments rouges) + papillon.
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x1C1C1E), 0);
        const ecu_data_t mock = {.connected = true, .rpm = 7200, .throttle = 85,
                                 .coolant_temp = 92, .battery_voltage = 14.0f,
                                 .oil_temp = 98};
        rpm_screen_update(rpm_screen_create(screen), &mock);
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
