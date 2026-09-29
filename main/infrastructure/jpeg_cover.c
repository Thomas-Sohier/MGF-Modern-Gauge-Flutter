#include "infrastructure/jpeg_cover.h"

#include <stdlib.h>
#include <string.h>

#include "domain/music_cover.h"

// tjpgd est le décodeur logiciel embarqué par LVGL (LV_USE_TJPGD). Le chemin
// relatif fonctionne des deux côtés : `src` est un dossier d'inclusion public
// de LVGL sur cible, et le simulateur le passe via -I.
#include "libs/tjpgd/tjpgd.h"

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

// Tampon de travail recommandé par TJpgDec.
#define JPEG_COVER_WORK_SIZE 4096U
// Bornes défensives : une pochette compagnon fait au plus 480x480.
#define JPEG_COVER_MAX_DIMENSION 1024
#define JPEG_COVER_MAX_PIXELS (1024U * 1024U)

typedef struct {
    const uint8_t *data;
    size_t length;
    size_t position;
    uint16_t *pixels; // image complète RGB565, allouée par le décodeur
    int width;
} jpeg_cover_ctx_t;

static void *cover_alloc(size_t size) {
#ifdef ESP_PLATFORM
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p != NULL) return p;
#endif
    return malloc(size);
}

static void cover_free(void *p) {
#ifdef ESP_PLATFORM
    heap_caps_free(p);
#else
    free(p);
#endif
}

static size_t tjpgd_input(JDEC *jd, uint8_t *buffer, size_t count) {
    jpeg_cover_ctx_t *ctx = jd->device;
    const size_t remaining = ctx->length - ctx->position;
    if (count > remaining) count = remaining;
    if (buffer != NULL) {
        memcpy(buffer, ctx->data + ctx->position, count);
    }
    ctx->position += count;
    return count;
}

static int tjpgd_output(JDEC *jd, void *bitmap, JRECT *rect) {
    jpeg_cover_ctx_t *ctx = jd->device;
    const uint8_t *src = bitmap; // RGB888 (3 octets/pixel)
    for (int y = rect->top; y <= (int)rect->bottom; y++) {
        uint16_t *row = ctx->pixels + (size_t)y * (size_t)ctx->width + rect->left;
        for (int x = rect->left; x <= (int)rect->right; x++) {
            const uint8_t r = *src++;
            const uint8_t g = *src++;
            const uint8_t b = *src++;
            *row++ = (uint16_t)(((uint16_t)(r & 0xF8U) << 8) |
                                ((uint16_t)(g & 0xFCU) << 3) | (b >> 3));
        }
    }
    return 1;
}

bool jpeg_cover_decode_amber(const uint8_t *jpeg, size_t length, int width,
                             int height, uint16_t *out) {
    if (jpeg == NULL || length < 4U || out == NULL || width <= 0 ||
        height <= 0 || width > MUSIC_COVER_WIDTH ||
        height > MUSIC_COVER_HEIGHT) {
        return false;
    }

    jpeg_cover_ctx_t ctx = {.data = jpeg, .length = length};
    JDEC jd;
    uint8_t work[JPEG_COVER_WORK_SIZE];
    if (jd_prepare(&jd, tjpgd_input, work, sizeof(work), &ctx) != JDR_OK) {
        return false;
    }
    if (jd.width == 0 || jd.height == 0 ||
        jd.width > JPEG_COVER_MAX_DIMENSION ||
        jd.height > JPEG_COVER_MAX_DIMENSION ||
        (size_t)jd.width * (size_t)jd.height > JPEG_COVER_MAX_PIXELS) {
        return false;
    }

    const size_t pixel_count = (size_t)jd.width * (size_t)jd.height;
    ctx.pixels = cover_alloc(pixel_count * sizeof(uint16_t));
    if (ctx.pixels == NULL) return false;
    ctx.width = jd.width;

    const JRESULT result = jd_decomp(&jd, tjpgd_output, 0);
    if (result != JDR_OK) {
        cover_free(ctx.pixels);
        return false;
    }

    const bool ok = music_cover_render(ctx.pixels, jd.width, jd.height, out,
                                       width, height);
    cover_free(ctx.pixels);
    return ok;
}
