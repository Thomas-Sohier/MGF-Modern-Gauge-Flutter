#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "domain/companion_art.h"
#include "domain/music_cover.h"

static void test_art_control(void) {
    companion_art_control_t c;
    const char *ok = "{\"art_id\":\"a1b2\",\"total_bytes\":4096,"
                     "\"chunk_count\":9}";
    assert(companion_parse_art_control(ok, strlen(ok), &c));
    assert(strcmp(c.art_id, "a1b2") == 0);
    assert(c.total_bytes == 4096);
    assert(c.chunk_count == 9);

    const char *bad[] = {
        "",
        "{}",
        "{\"art_id\":\"x\",\"total_bytes\":10}",
        "{\"art_id\":\"x\",\"chunk_count\":1}",
        "{\"total_bytes\":10,\"chunk_count\":1}",
        "{\"art_id\":\"\",\"total_bytes\":10,\"chunk_count\":1}",
        "{\"art_id\":null,\"total_bytes\":10,\"chunk_count\":1}",
        "{\"art_id\":1,\"total_bytes\":10,\"chunk_count\":1}",
        "{\"art_id\":\"x\",\"total_bytes\":0,\"chunk_count\":1}",
        "{\"art_id\":\"x\",\"total_bytes\":10,\"chunk_count\":0}",
        // chunk_count > total_bytes : au moins un octet utile par chunk.
        "{\"art_id\":\"x\",\"total_bytes\":3,\"chunk_count\":4}",
        "{\"art_id\":\"x\",\"total_bytes\":262145,\"chunk_count\":1}",
        "{\"art_id\":\"x\",\"total_bytes\":10.5,\"chunk_count\":1}",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        assert(!companion_parse_art_control(bad[i], strlen(bad[i]), &c));
    }
    assert(!companion_parse_art_control(NULL, 3, &c));

    // Identifiant trop long rejeté.
    char long_id[128];
    memset(long_id, 'a', sizeof(long_id));
    char json[256];
    snprintf(json, sizeof(json),
             "{\"art_id\":\"%s\",\"total_bytes\":4,\"chunk_count\":1}",
             long_id);
    assert(!companion_parse_art_control(json, strlen(json), &c));
}

static void test_reassembler(void) {
    uint8_t buffer[32];
    companion_art_reassembler_t r;
    companion_art_reassembler_init(&r, buffer, sizeof(buffer));

    // Rien n'est actif au départ.
    assert(!companion_art_reassembler_complete(&r));
    assert(!companion_art_reassembler_push(&r, (const uint8_t *)"\x00\x00x", 3));

    // Deux chunks séquentiels : 3 + 2 octets.
    companion_art_control_t control = {
        .art_id = "x", .total_bytes = 5, .chunk_count = 2};
    assert(companion_art_reassembler_begin(&r, &control));
    const uint8_t chunk0[] = {0x00, 0x00, 'a', 'b', 'c'};
    assert(companion_art_reassembler_push(&r, chunk0, sizeof(chunk0)));
    assert(!companion_art_reassembler_complete(&r));
    // Chunk 0 en double : rejeté.
    assert(!companion_art_reassembler_push(&r, chunk0, sizeof(chunk0)));
    // Index hors bornes : rejeté.
    const uint8_t bad_index[] = {0x00, 0x02, 'z'};
    assert(!companion_art_reassembler_push(&r, bad_index, sizeof(bad_index)));
    // Chunk final, longueur attendue 2.
    const uint8_t chunk1[] = {0x00, 0x01, 'd', 'e'};
    assert(companion_art_reassembler_push(&r, chunk1, sizeof(chunk1)));
    assert(companion_art_reassembler_complete(&r));
    assert(memcmp(buffer, "abcde", 5) == 0);

    // Un chunk arrivé après le 0 mais dans le désordre reste accepté tant que
    // la taille des chunks pleins est connue et les longueurs cohérentes.
    companion_art_control_t three = {
        .art_id = "y", .total_bytes = 5, .chunk_count = 3};
    companion_art_reassembler_init(&r, buffer, sizeof(buffer));
    assert(companion_art_reassembler_begin(&r, &three));
    const uint8_t first[] = {0x00, 0x00, 'a', 'b'};
    const uint8_t last[] = {0x00, 0x02, 'e'};
    const uint8_t middle[] = {0x00, 0x01, 'c', 'd'};
    assert(companion_art_reassembler_push(&r, first, sizeof(first)));
    // Avant le chunk 0, un index ultérieur est refusé : nouvelle session.
    assert(companion_art_reassembler_begin(&r, &three));
    assert(!companion_art_reassembler_push(&r, middle, sizeof(middle)));
    assert(companion_art_reassembler_push(&r, first, sizeof(first)));
    assert(companion_art_reassembler_push(&r, last, sizeof(last)));
    assert(companion_art_reassembler_push(&r, middle, sizeof(middle)));
    assert(companion_art_reassembler_complete(&r));
    assert(memcmp(buffer, "abcde", 5) == 0);

    // Taille incohérente au milieu.
    companion_art_reassembler_init(&r, buffer, sizeof(buffer));
    const companion_art_control_t two = {
        .art_id = "z", .total_bytes = 5, .chunk_count = 2};
    assert(companion_art_reassembler_begin(&r, &two));
    const uint8_t short0[] = {0x00, 0x00, 'a', 'b', 'c'};
    const uint8_t long1[] = {0x00, 0x01, 'd', 'e', 'f'};
    assert(companion_art_reassembler_push(&r, short0, sizeof(short0)));
    assert(!companion_art_reassembler_push(&r, long1, sizeof(long1)));

    // Dépassement de cap / taille annoncée au-delà du chunk 0.
    companion_art_reassembler_init(&r, buffer, sizeof(buffer));
    const companion_art_control_t odd = {
        .art_id = "w", .total_bytes = 5, .chunk_count = 1};
    assert(companion_art_reassembler_begin(&r, &odd));
    const uint8_t one[] = {0x00, 0x00, 'a', 'b', 'c'};
    assert(!companion_art_reassembler_push(&r, one, sizeof(one)));

    // Contrôle plus grand que le tampon : refusé.
    companion_art_reassembler_init(&r, buffer, 4);
    assert(!companion_art_reassembler_begin(&r, &control));

    // Chunk vide / trop long.
    companion_art_reassembler_init(&r, buffer, sizeof(buffer));
    assert(companion_art_reassembler_begin(&r, &two));
    const uint8_t too_short[] = {0x00, 0x00};
    assert(!companion_art_reassembler_push(&r, too_short, sizeof(too_short)));
}

static void test_cover_palette(void) {
    // Noir -> fond, blanc -> ambre (sans tramage, niveau extrême).
    const uint16_t bg = music_cover_amber_pixel(0, 0, 0, 0);
    const uint16_t amber = music_cover_amber_pixel(255, 255, 255, 0);
    assert(bg == 0x18A2);   // #1B1712 en RGB565
    assert(amber == 0xFDA3); // #FFB51B en RGB565
    // Monotone sur la luminance (un gris moyen tombe strictement entre).
    const uint16_t mid = music_cover_amber_pixel(128, 128, 128, 0);
    assert(mid != bg && mid != amber);
}

static void test_cover_crop(void) {
    // Source 4x2 (paysage) : colonnes noires à gauche, blanches à droite.
    // Le cadrage « cover » carré centré garde les colonnes 1 et 2.
    uint16_t src[4 * 2];
    for (int y = 0; y < 2; y++) {
        src[y * 4 + 0] = 0x0000;
        src[y * 4 + 1] = 0x0000;
        src[y * 4 + 2] = 0xFFFF;
        src[y * 4 + 3] = 0xFFFF;
    }
    uint16_t out[2 * 2];
    assert(music_cover_render(src, 4, 2, out, 2, 2));
    const int d00 = 0;  // Bayer (0,0)
    const int d01 = 8;  // Bayer (0,1)
    assert(out[0] == music_cover_amber_pixel(0, 0, 0, d00));
    assert(out[1] == music_cover_amber_pixel(255, 255, 255, d01));
    assert(out[2] == music_cover_amber_pixel(0, 0, 0, 12));   // Bayer (1,0)
    assert(out[3] == music_cover_amber_pixel(255, 255, 255, 4)); // (1,1)

    // Même ratio que la source : aucun crop, échantillonnage 1:1.
    uint16_t out_wide[4 * 2];
    assert(music_cover_render(src, 4, 2, out_wide, 4, 2));
    assert(out_wide[1] == music_cover_amber_pixel(0, 0, 0, 8));
    assert(out_wide[2] == music_cover_amber_pixel(255, 255, 255, 2));

    // Source 2x4 (portrait) : les lignes extrêmes sont noires, le milieu blanc.
    // Le cadrage carré garde les lignes 1 et 2 (blanches).
    uint16_t tall[2 * 4];
    for (int y = 0; y < 4; y++) {
        const uint16_t v = (y == 1 || y == 2) ? 0xFFFF : 0x0000;
        tall[y * 2 + 0] = v;
        tall[y * 2 + 1] = v;
    }
    assert(music_cover_render(tall, 2, 4, out, 2, 2));
    assert(out[0] == music_cover_amber_pixel(255, 255, 255, d00));
    assert(out[3] == music_cover_amber_pixel(255, 255, 255, 4));

    // Arguments invalides.
    assert(!music_cover_render(NULL, 4, 2, out, 2, 2));
    assert(!music_cover_render(src, 4, 2, NULL, 2, 2));
    assert(!music_cover_render(src, 0, 2, out, 2, 2));
    assert(!music_cover_render(src, 4, 2, out, 0, 2));
    assert(!music_cover_render(src, 4, 2, out, 2, 0));
    assert(!music_cover_render(src, 4, 2, out, MUSIC_COVER_WIDTH + 1, 2));
    assert(!music_cover_render(src, 4, 2, out, 2, MUSIC_COVER_HEIGHT + 1));
}

int main(void) {
    test_art_control();
    test_reassembler();
    test_cover_palette();
    test_cover_crop();
    printf("test_music_cover: OK\n");
    return 0;
}
