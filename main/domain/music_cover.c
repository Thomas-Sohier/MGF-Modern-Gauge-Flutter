#include "domain/music_cover.h"

#include <string.h>

#define COVER_LEVELS 16

// Matrice de Bayer 4x4 (0..15), seuils de tramage ordonné.
static const uint8_t k_bayer[16] = {
    0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5,
};

static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((uint16_t)(r & 0xF8U) << 8) | ((g & 0xFCU) << 3) |
                      (b >> 3));
}

static uint8_t expand5(uint16_t v) {
    return (uint8_t)((v << 3) | (v >> 2));
}

static uint8_t expand6(uint16_t v) {
    return (uint8_t)((v << 2) | (v >> 4));
}

// Fond #1B1712, ambre #FFB51B.
static uint16_t amber_level_color(int level) {
    if (level < 0) level = 0;
    if (level > COVER_LEVELS - 1) level = COVER_LEVELS - 1;
    static const uint8_t bg[3] = {0x1B, 0x17, 0x12};
    static const uint8_t amber[3] = {0xFF, 0xB5, 0x1B};
    uint8_t rgb[3];
    for (int c = 0; c < 3; c++) {
        rgb[c] = (uint8_t)((bg[c] * (COVER_LEVELS - 1 - level) +
                            amber[c] * level) /
                           (COVER_LEVELS - 1));
    }
    return rgb565(rgb[0], rgb[1], rgb[2]);
}

uint16_t music_cover_amber_pixel(uint8_t r, uint8_t g, uint8_t b, int dither) {
    const int luminance = (77 * r + 150 * g + 29 * b) >> 8; // 0..255
    const int scaled = luminance * (COVER_LEVELS - 1);      // 0..3825
    int level = scaled / 255;
    const int frac = scaled % 255;
    const int threshold = (dither & 15) * 255;
    if (frac * 16 > threshold) level++;
    return amber_level_color(level);
}

bool music_cover_render(const uint16_t *src, int src_width, int src_height,
                        uint16_t *out, int width, int height) {
    if (src == NULL || out == NULL || src_width <= 0 || src_height <= 0 ||
        width <= 0 || height <= 0 || width > MUSIC_COVER_WIDTH ||
        height > MUSIC_COVER_HEIGHT) {
        return false;
    }

    // Cadrage « cover » : on garde le ratio de la source et on rogne l'axe qui
    // dépasse par rapport au ratio de la cible, centré sur l'image.
    int crop_w = src_width;
    int crop_h = src_height;
    if ((int64_t)src_width * height > (int64_t)src_height * width) {
        // Source relativement plus large : rogne la largeur.
        crop_w = (int)(((int64_t)src_height * width + height / 2) / height);
    } else {
        // Source relativement plus haute : rogne la hauteur.
        crop_h = (int)(((int64_t)src_width * height + width / 2) / width);
    }
    if (crop_w > src_width) crop_w = src_width;
    if (crop_h > src_height) crop_h = src_height;
    if (crop_w <= 0) crop_w = 1;
    if (crop_h <= 0) crop_h = 1;
    const int crop_x = (src_width - crop_w) / 2;
    const int crop_y = (src_height - crop_h) / 2;

    for (int ty = 0; ty < height; ty++) {
        // Échantillonnage centré, entier et déterministe.
        const int sy = crop_y + ((2 * ty + 1) * crop_h) / (2 * height);
        const uint16_t *row = src + (size_t)sy * (size_t)src_width;
        for (int tx = 0; tx < width; tx++) {
            const int sx = crop_x + ((2 * tx + 1) * crop_w) / (2 * width);
            const uint16_t pixel = row[sx];
            const uint8_t r = expand5((pixel >> 11) & 0x1FU);
            const uint8_t g = expand6((pixel >> 5) & 0x3FU);
            const uint8_t b = expand5(pixel & 0x1FU);
            const int dither = k_bayer[((ty & 3) << 2) | (tx & 3)];
            out[(size_t)ty * (size_t)width + tx] =
                music_cover_amber_pixel(r, g, b, dither);
        }
    }
    return true;
}
