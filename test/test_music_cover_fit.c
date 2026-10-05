// Tests hôte du cadrage « cover » de la pochette (ui/screens/music_cover_fit.h).
#include <math.h>
#include <stdio.h>

#include "domain/music_cover.h"
#include "ui/screens/music_cover_fit.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "%s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond);  \
            failures++;                                                        \
        }                                                                      \
    } while (0)

// Propriétés d'un cadrage « cover » valide : couvre les deux axes avec le
// recouvrement, échelle minimale (un cran de moins ne couvre plus), et
// dimensions dérivées d'un facteur unique (aucune déformation).
static void check_cover(int32_t sw, int32_t sh, int32_t bw, int32_t bh) {
    music_cover_fit_t fit;
    CHECK(music_cover_fit(sw, sh, bw, bh, &fit));
    const int32_t need_w = bw + 2 * MUSIC_COVER_FIT_BLEED_PX;
    const int32_t need_h = bh + 2 * MUSIC_COVER_FIT_BLEED_PX;
    CHECK(fit.width >= need_w);
    CHECK(fit.height >= need_h);
    CHECK(fit.width == (int32_t)((int64_t)sw * fit.scale / 256));
    CHECK(fit.height == (int32_t)((int64_t)sh * fit.scale / 256));
    const uint32_t smaller = fit.scale - 1;
    CHECK((int64_t)sw * smaller < (int64_t)need_w * 256 ||
          (int64_t)sh * smaller < (int64_t)need_h * 256);
    // Ratio conservé à l'arrondi entier près.
    const double src_ratio = (double)sw / (double)sh;
    const double out_ratio = (double)fit.width / (double)fit.height;
    CHECK(fabs(out_ratio - src_ratio) <= src_ratio * 2.0 / fit.height + 1e-9);
}

static void test_invalid(void) {
    music_cover_fit_t fit;
    CHECK(!music_cover_fit(0, 10, 10, 10, &fit));
    CHECK(!music_cover_fit(10, -1, 10, 10, &fit));
    CHECK(!music_cover_fit(10, 10, 0, 10, &fit));
    CHECK(!music_cover_fit(10, 10, 10, 0, &fit));
    CHECK(!music_cover_fit(10, 10, 10, 10, NULL));
}

static void test_shapes(void) {
    // Source décodée actuelle dans la fenêtre à 480, 320 et d'autres tailles.
    check_cover(MUSIC_COVER_WIDTH, MUSIC_COVER_HEIGHT, 458, 323);
    check_cover(MUSIC_COVER_WIDTH, MUSIC_COVER_HEIGHT, 305, 216);
    // Sources plus larges, plus hautes, carrées : jamais d'étirement.
    check_cover(1000, 200, 458, 323);
    check_cover(200, 1000, 458, 323);
    check_cover(300, 300, 458, 323);
    check_cover(1, 1, 458, 323);
    check_cover(4000, 4000, 20, 10);
}

static void test_wide_source_crops_sides(void) {
    // Source plus large que la fenêtre : la hauteur pilote, la largeur
    // déborde (rognée symétriquement par le centrage).
    music_cover_fit_t fit;
    CHECK(music_cover_fit(800, 200, 400, 200, &fit));
    CHECK(fit.height >= 202 && fit.height <= 203);
    CHECK(fit.width > 400 + 2);
}

static void test_tall_source_crops_top_bottom(void) {
    music_cover_fit_t fit;
    CHECK(music_cover_fit(200, 800, 400, 200, &fit));
    CHECK(fit.width >= 402 && fit.width <= 403);
    CHECK(fit.height > 200 + 2);
}

static void test_geometry(void) {
    // Le disque de découpe reste sous l'anneau (qui recouvre la couture) sans
    // déborder de son bord extérieur.
    const float ring_in = MUSIC_RING_R - MUSIC_RING_W * 0.5f;
    const float ring_out = MUSIC_RING_R + MUSIC_RING_W * 0.5f;
    CHECK(MUSIC_COVER_CLIP_R > ring_in);
    CHECK(MUSIC_COVER_CLIP_R < ring_out);
    CHECK(fabsf(MUSIC_COVER_CLIP_XY + MUSIC_COVER_CLIP_R - 160.0f) < 1e-4f);
    // La fenêtre image s'arrête avant le titre (capitale ~7 px au-dessus).
    CHECK(MUSIC_COVER_IMAGE_Y1 <= MUSIC_TRACK_Y - 7.0f - 6.0f);
    CHECK(MUSIC_TRACK_Y < MUSIC_ARTIST_Y && MUSIC_ARTIST_Y < MUSIC_RULE_Y &&
          MUSIC_RULE_Y < MUSIC_TIME_Y);
    // Chaque ligne tient dans la corde du disque intérieur à sa base.
    const float cy = 160.0f;
    const float track_bottom = MUSIC_TRACK_Y + 7.0f;
    const float artist_bottom = MUSIC_ARTIST_Y + 5.0f;
    CHECK(2.0f * sqrtf(ring_in * ring_in -
                       (track_bottom - cy) * (track_bottom - cy)) >=
          MUSIC_TRACK_WIDTH);
    CHECK(2.0f * sqrtf(ring_in * ring_in -
                       (artist_bottom - cy) * (artist_bottom - cy)) >=
          MUSIC_ARTIST_WIDTH);
    CHECK(MUSIC_TIME_Y + 6.0f < cy + ring_in);
}

int main(void) {
    test_invalid();
    test_shapes();
    test_wide_source_crops_sides();
    test_tall_source_crops_top_bottom();
    test_geometry();
    if (failures != 0) {
        fprintf(stderr, "test_music_cover_fit: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_music_cover_fit: OK\n");
    return 0;
}
