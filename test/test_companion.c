#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "domain/companion_protocol.h"
#include "domain/flat_json.h"
#include "domain/rtc_time.h"

#define PARSE(fn, text, out) fn(text, strlen(text), out)

static int g_members;
static bool count_member(void *c, const char *k, const flat_json_value_t *v) {
    (void)c;
    (void)k;
    (void)v;
    g_members++;
    return true;
}

static void test_flat_json(void) {
    char scratch[64];
    const char *ok =
        " {\"a\":\"x\\u00e9\\ud83d\\ude00\",\"b\":-1.5e2,\"c\":true,"
        "\"d\":null,\"e\":false} ";
    g_members = 0;
    assert(flat_json_parse(ok, strlen(ok), scratch, sizeof(scratch),
                           count_member, NULL));
    assert(g_members == 5);
    assert(
        flat_json_parse("{}", 2, scratch, sizeof(scratch), count_member, NULL));
    const char *bad[] = {
        "",
        "{",
        "{\"a\":}",
        "{\"a\":[1]}",
        "{\"a\":{}}",
        "{\"a\":1,}",
        "{\"a\":\"\\ud83d\"}",
        "{\"a\":\"x\"} x",
        "{\"a\":01x}",
        "[1]",
        "{\"a\" 1}",
        "{\"a\":\"\x01\"}",
        "{\"a\":\"\xC3\"}",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        assert(!flat_json_parse(bad[i], strlen(bad[i]), scratch,
                                sizeof(scratch), count_member, NULL));
    }
    // Terminateur NUL final toléré (certaines piles l'ajoutent).
    assert(flat_json_parse("{}\0", 3, scratch, sizeof(scratch), count_member,
                           NULL));
}

static void test_display_text(void) {
    char out[32];
    companion_display_text("  Tournez à droite\u202fsur l'Été ", out,
                           sizeof(out));
    assert(strcmp(out, "TOURNEZ A DROITE SUR L'ETE") == 0);
    companion_display_text("Cœur \xF0\x9F\x8E\xB5 “Déjà” · 5 km", out,
                           sizeof(out));
    assert(strcmp(out, "COEUR \"DEJA\" - 5 KM") == 0);
    // Troncature sans dépasser le tampon.
    char small[6];
    companion_display_text("abcdefghij", small, sizeof(small));
    assert(strcmp(small, "ABCDE") == 0);
    companion_display_text(NULL, out, sizeof(out));
    assert(out[0] == '\0');
}

static void test_media(void) {
    companion_media_t m;
    assert(PARSE(companion_parse_media,
                 "{\"title\":\"Midnight Drive\",\"artist\":\"Café Noir\","
                 "\"album\":\"x\",\"state\":\"playing\",\"position_ms\":142000,"
                 "\"duration_ms\":228000,\"art_id\":null}",
                 &m));
    assert(strcmp(m.title, "MIDNIGHT DRIVE") == 0);
    assert(strcmp(m.artist, "CAFE NOIR") == 0);
    assert(m.state == COMPANION_PLAYBACK_PLAYING);
    assert(m.position_ms == 142000 && m.duration_ms == 228000);
    assert(companion_media_position_at(&m, 1000, 3500) == 144500);
    assert(companion_media_position_at(&m, 1000, 1000 + 200000) == 228000);
    m.state = COMPANION_PLAYBACK_PAUSED;
    assert(companion_media_position_at(&m, 1000, 9000) == 142000);

    assert(!PARSE(companion_parse_media, "{\"title\":\"x\"}", &m));
    assert(!PARSE(companion_parse_media,
                  "{\"title\":\"x\",\"state\":\"buffering\"}", &m));
    assert(!PARSE(companion_parse_media,
                  "{\"title\":\"x\",\"state\":\"paused\",\"position_ms\":1.5}",
                  &m));
}

static void test_nav(void) {
    companion_nav_t n;
    assert(
        PARSE(companion_parse_nav,
              "{\"active\":true,\"instruction\":\"Tournez à droite sur Rue des "
              "Lilas\",\"distance\":\"300\u00a0m\",\"eta\":\"Arrivée 14:32\","
              "\"maneuver_icon_id\":\"ab12cd34\"}",
              &n));
    assert(n.active);
    assert(strcmp(n.instruction, "TOURNEZ A DROITE") == 0);
    assert(strcmp(n.street, "RUE DES LILAS") == 0);
    assert(strcmp(n.distance_value, "300") == 0);
    assert(strcmp(n.distance_unit, "m") == 0);
    assert(strcmp(n.eta, "ARRIVEE 14:32") == 0);
    assert(n.maneuver == COMPANION_MANEUVER_RIGHT);

    assert(
        PARSE(companion_parse_nav,
              "{\"active\":true,\"instruction\":null,\"distance\":\"1,2 km\","
              "\"eta\":null,\"maneuver_icon_id\":null}",
              &n));
    assert(strcmp(n.distance_value, "1,2") == 0);
    assert(strcmp(n.distance_unit, "km") == 0);
    assert(n.instruction[0] == '\0' &&
           n.maneuver == COMPANION_MANEUVER_UNKNOWN);

    assert(PARSE(
        companion_parse_nav,
        "{\"active\":true,\"instruction\":\"x\",\"distance\":\"Maintenant\"}",
        &n));
    assert(strcmp(n.distance_value, "MAINTENANT") == 0 &&
           n.distance_unit[0] == '\0');

    assert(PARSE(companion_parse_nav, "{\"active\":false}", &n));
    assert(!n.active);
    assert(!PARSE(companion_parse_nav, "{\"instruction\":\"x\"}", &n));
}

static void test_maneuver(void) {
    assert(companion_maneuver_from_text("TOURNEZ A GAUCHE") ==
           COMPANION_MANEUVER_LEFT);
    assert(companion_maneuver_from_text("SERREZ A DROITE") ==
           COMPANION_MANEUVER_SLIGHT_RIGHT);
    assert(companion_maneuver_from_text("TOURNEZ LEGEREMENT A GAUCHE") ==
           COMPANION_MANEUVER_SLIGHT_LEFT);
    assert(companion_maneuver_from_text("TOURNEZ FRANCHEMENT A DROITE") ==
           COMPANION_MANEUVER_SHARP_RIGHT);
    assert(companion_maneuver_from_text("CONTINUEZ TOUT DROIT") ==
           COMPANION_MANEUVER_STRAIGHT);
    assert(companion_maneuver_from_text("FAITES DEMI-TOUR") ==
           COMPANION_MANEUVER_UTURN);
    assert(companion_maneuver_from_text("AU ROND-POINT, PRENEZ LA 2E SORTIE") ==
           COMPANION_MANEUVER_ROUNDABOUT);
    assert(companion_maneuver_from_text("VOUS ETES ARRIVE") ==
           COMPANION_MANEUVER_ARRIVE);
    assert(companion_maneuver_from_text("TURN RIGHT") ==
           COMPANION_MANEUVER_RIGHT);
    assert(companion_maneuver_from_text("") == COMPANION_MANEUVER_UNKNOWN);
}

static void test_key_and_time(void) {
    companion_key_t k;
    assert(PARSE(companion_parse_key,
                 "{\"type\":\"nav_key\",\"key\":\"previous\"}", &k));
    assert(k == COMPANION_KEY_PREVIOUS);
    assert(!PARSE(companion_parse_key, "{\"type\":\"other\",\"key\":\"next\"}",
                  &k));
    assert(!PARSE(companion_parse_key,
                  "{\"type\":\"nav_key\",\"key\":\"jump\"}", &k));

    companion_time_t t;
    assert(PARSE(companion_parse_time,
                 "{\"epoch_ms\":1758619800000,\"tz_offset_min\":120}", &t));
    assert(t.epoch_ms == 1758619800000LL && t.utc_offset_min == 120);
    assert(!PARSE(companion_parse_time, "{\"epoch_ms\":1,\"tz_offset_min\":7}",
                  &t));
    assert(!PARSE(companion_parse_time,
                  "{\"epoch_ms\":1,\"tz_offset_min\":900}", &t));
    assert(!PARSE(companion_parse_time, "{\"epoch_ms\":-5,\"tz_offset_min\":0}",
                  &t));
}

static void test_unix_conversions(void) {
    rtc_datetime_t dt;
    // 2025-09-23 09:30:00 UTC, mardi (3).
    assert(rtc_datetime_from_unix(1758619800, &dt));
    assert(dt.year == 2025 && dt.month == 9 && dt.day == 23);
    assert(dt.hour == 9 && dt.minute == 30 && dt.second == 0 &&
           dt.weekday == 3);
    int64_t back;
    assert(rtc_datetime_to_unix(&dt, &back) && back == 1758619800);
    // 2000-01-01 samedi (7), 2024-02-29 jeudi (5).
    assert(rtc_datetime_from_unix(946684800, &dt) && dt.weekday == 7);
    assert(rtc_datetime_from_unix(1709164800, &dt));
    assert(dt.month == 2 && dt.day == 29 && dt.weekday == 5);
    assert(!rtc_datetime_from_unix(946684799, &dt));    // 1999
    assert(!rtc_datetime_from_unix(4102444800LL, &dt)); // 2100

    // Passage de jour avec le décalage local.
    rtc_datetime_t utc = {2025, 12, 31, 4, 23, 30, 0};
    rtc_datetime_t local;
    assert(rtc_datetime_add_minutes(&utc, 60, &local));
    assert(local.year == 2026 && local.month == 1 && local.day == 1 &&
           local.hour == 0 && local.minute == 30 && local.weekday == 5);
    assert(rtc_datetime_add_minutes(&utc, -600, &local));
    assert(local.day == 31 && local.hour == 13);
}

int main(void) {
    test_flat_json();
    test_display_text();
    test_media();
    test_nav();
    test_maneuver();
    test_key_and_time();
    test_unix_conversions();
    puts("companion protocol tests: OK");
    return 0;
}
