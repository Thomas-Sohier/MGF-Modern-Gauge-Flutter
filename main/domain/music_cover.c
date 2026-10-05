#include "domain/music_cover.h"

#include "domain/amber_palette.h"

#include <string.h>

#define COVER_LEVELS MUSIC_COVER_LEVELS

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

static void palette_channels(uint32_t rgb, uint8_t out[3]) {
    out[0] = (uint8_t)((rgb >> 16) & 0xFFU);
    out[1] = (uint8_t)((rgb >> 8) & 0xFFU);
    out[2] = (uint8_t)(rgb & 0xFFU);
}

uint16_t music_cover_level_color(int level, bool inverted) {
    if (level < 0) level = 0;
    if (level > COVER_LEVELS - 1) level = COVER_LEVELS - 1;
    uint8_t bg[3];
    uint8_t amber[3];
    palette_channels(amber_palette_rgb(inverted, AMBER_PALETTE_ROLE_BG), bg);
    palette_channels(amber_palette_rgb(inverted, AMBER_PALETTE_ROLE_BRIGHT),
                     amber);
    uint8_t rgb[3];
    for (int c = 0; c < 3; c++) {
        rgb[c] =
            (uint8_t)((bg[c] * (COVER_LEVELS - 1 - level) + amber[c] * level) /
                      (COVER_LEVELS - 1));
    }
    return rgb565(rgb[0], rgb[1], rgb[2]);
}

static uint16_t amber_level_color(int level) {
    return music_cover_level_color(level, false);
}

size_t music_cover_convert_palette(uint16_t *pixels, size_t count,
                                   bool from_inverted, bool to_inverted) {
    if (pixels == NULL || count == 0) return 0;

    uint16_t from[COVER_LEVELS];
    uint16_t to[COVER_LEVELS];
    for (int level = 0; level < COVER_LEVELS; level++) {
        from[level] = music_cover_level_color(level, from_inverted);
        to[level] = music_cover_level_color(level, to_inverted);
    }

    // Index rapide par composante rouge (5 bits) : les niveaux de la palette
    // ont des rouges distincts. En cas de collision (palette future), on
    // retombe sur une recherche linéaire exacte.
    int8_t by_red[32];
    memset(by_red, -1, sizeof(by_red));
    bool indexed = true;
    for (int level = 0; level < COVER_LEVELS && indexed; level++) {
        const unsigned red = from[level] >> 11;
        if (by_red[red] >= 0) indexed = false;
        by_red[red] = (int8_t)level;
    }

    size_t matched = 0;
    for (size_t i = 0; i < count; i++) {
        const uint16_t pixel = pixels[i];
        int level = -1;
        if (indexed) {
            const int candidate = by_red[pixel >> 11];
            if (candidate >= 0 && from[candidate] == pixel) level = candidate;
        } else {
            for (int l = 0; l < COVER_LEVELS; l++) {
                if (from[l] == pixel) {
                    level = l;
                    break;
                }
            }
        }
        if (level < 0) continue;
        pixels[i] = to[level];
        matched++;
    }
    return matched;
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
