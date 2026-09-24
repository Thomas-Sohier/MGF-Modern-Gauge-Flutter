#include "domain/companion_protocol.h"

#include <math.h>
#include <string.h>

#include "domain/flat_json.h"

#define SCRATCH_SIZE (2U * (COMPANION_PAYLOAD_MAX + 1U))

// ── Normalisation du texte ──────────────────────────────────────────────────
typedef struct {
    char *data;
    size_t size;
    size_t length;
    bool pending_space;
} text_out_t;

static void emit_char(text_out_t *out, char c) {
    if (c == ' ') {
        // Fusionne les espaces et ignore ceux de tête.
        if (out->length > 0) out->pending_space = true;
        return;
    }
    const size_t needed = out->pending_space ? 2U : 1U;
    if (out->length + needed >= out->size) return;
    if (out->pending_space) {
        out->data[out->length++] = ' ';
        out->pending_space = false;
    }
    out->data[out->length++] = c;
}

static void emit_string(text_out_t *out, const char *s) {
    for (; *s != '\0'; s++) emit_char(out, *s);
}

static char ascii_upper(char c) {
    return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

// Repli d'un point de code non ASCII ; NULL : caractère retiré.
static const char *fold_codepoint(uint32_t cp) {
    // Latin-1 (U+00C0..U+00FF), indexé à partir de U+00C0.
    static const char *const latin1[64] = {
        "A", "A", "A", "A", "A", "A", "AE", "C",   // C0-C7
        "E", "E", "E", "E", "I", "I", "I", "I",    // C8-CF
        "D", "N", "O", "O", "O", "O", "O", "X",    // D0-D7
        "O", "U", "U", "U", "U", "Y", "TH", "SS",  // D8-DF
        "A", "A", "A", "A", "A", "A", "AE", "C",   // E0-E7
        "E", "E", "E", "E", "I", "I", "I", "I",    // E8-EF
        "D", "N", "O", "O", "O", "O", "O", "/",    // F0-F7
        "O", "U", "U", "U", "U", "Y", "TH", "Y",   // F8-FF
    };
    if (cp >= 0xC0u && cp <= 0xFFu) return latin1[cp - 0xC0u];
    switch (cp) {
    case 0x00A0u: return " ";  // espace insécable
    case 0x202Fu: return " ";  // espace fine insécable (typographie FR)
    case 0x2009u: return " ";
    case 0x00B7u: return "-";  // point médian (« 12 min · 5 km »)
    case 0x2022u: return "-";
    case 0x2013u: return "-";
    case 0x2014u: return "-";
    case 0x2018u: return "'";
    case 0x2019u: return "'";
    case 0x201Cu: return "\"";
    case 0x201Du: return "\"";
    case 0x00ABu: return "\"";
    case 0x00BBu: return "\"";
    case 0x2026u: return "...";
    case 0x0152u: return "OE";
    case 0x0153u: return "OE";
    case 0x0178u: return "Y";
    case 0x2192u: return "-";  // flèche
    default: return NULL;
    }
}

void companion_display_text(const char *utf8, char *out, size_t out_size) {
    if (out == NULL || out_size == 0) return;
    out[0] = '\0';
    if (utf8 == NULL) return;
    text_out_t text = {.data = out, .size = out_size};
    const unsigned char *s = (const unsigned char *)utf8;
    while (*s != '\0') {
        const unsigned char c = *s;
        if (c < 0x80u) {
            if (c == '\t' || c == '\n' || c == '\r') emit_char(&text, ' ');
            else if (c >= 0x20u && c < 0x7Fu) emit_char(&text, ascii_upper((char)c));
            s++;
            continue;
        }
        uint32_t cp;
        size_t count;
        if ((c & 0xE0u) == 0xC0u) { cp = c & 0x1Fu; count = 2; }
        else if ((c & 0xF0u) == 0xE0u) { cp = c & 0x0Fu; count = 3; }
        else if ((c & 0xF8u) == 0xF0u) { cp = c & 0x07u; count = 4; }
        else { s++; continue; } // octet de continuation isolé
        size_t i = 1;
        for (; i < count && (s[i] & 0xC0u) == 0x80u; i++) {
            cp = (cp << 6) | (s[i] & 0x3Fu);
        }
        s += i;
        if (i != count) continue; // séquence tronquée : ignorée
        const char *folded = fold_codepoint(cp);
        if (folded != NULL) emit_string(&text, folded);
    }
    out[text.length] = '\0';
}

// ── Décodage des messages ───────────────────────────────────────────────────
static bool parse(const char *json, size_t length, flat_json_member_cb_t cb,
                  void *context) {
    char scratch[SCRATCH_SIZE];
    if (json == NULL || length == 0 || length > COMPANION_PAYLOAD_MAX) {
        return false;
    }
    return flat_json_parse(json, length, scratch, sizeof(scratch), cb, context);
}

static bool is_integer(const flat_json_value_t *v) {
    return v->type == FLAT_JSON_NUMBER && isfinite(v->number) &&
           fabs(v->number) < 9.0e15 && floor(v->number) == v->number;
}

typedef struct {
    companion_media_t media;
    bool has_title;
    bool has_state;
} media_parse_t;

static bool media_member(void *context, const char *key,
                         const flat_json_value_t *v) {
    media_parse_t *p = context;
    if (strcmp(key, "title") == 0) {
        if (v->type == FLAT_JSON_STRING) {
            companion_display_text(v->string, p->media.title,
                                   sizeof(p->media.title));
        } else if (v->type != FLAT_JSON_NULL) {
            return false;
        }
        p->has_title = true;
    } else if (strcmp(key, "artist") == 0) {
        if (v->type == FLAT_JSON_STRING) {
            companion_display_text(v->string, p->media.artist,
                                   sizeof(p->media.artist));
        } else if (v->type != FLAT_JSON_NULL) {
            return false;
        }
    } else if (strcmp(key, "state") == 0) {
        if (v->type != FLAT_JSON_STRING) return false;
        if (strcmp(v->string, "playing") == 0) {
            p->media.state = COMPANION_PLAYBACK_PLAYING;
        } else if (strcmp(v->string, "paused") == 0) {
            p->media.state = COMPANION_PLAYBACK_PAUSED;
        } else if (strcmp(v->string, "stopped") == 0) {
            p->media.state = COMPANION_PLAYBACK_STOPPED;
        } else {
            return false;
        }
        p->has_state = true;
    } else if (strcmp(key, "position_ms") == 0) {
        if (v->type == FLAT_JSON_NULL) return true;
        if (!is_integer(v)) return false;
        p->media.position_ms = v->number < 0 ? -1 : (int64_t)v->number;
    } else if (strcmp(key, "duration_ms") == 0) {
        if (v->type == FLAT_JSON_NULL) return true;
        if (!is_integer(v)) return false;
        p->media.duration_ms = (int64_t)v->number;
    }
    // Autres membres (album, art_id…) : ignorés, compatibilité ascendante.
    return true;
}

bool companion_parse_media(const char *json, size_t length,
                           companion_media_t *out) {
    media_parse_t p = {.media = {.position_ms = -1, .duration_ms = 0}};
    if (out == NULL || !parse(json, length, media_member, &p) ||
        !p.has_title || !p.has_state) {
        return false;
    }
    *out = p.media;
    return true;
}

typedef struct {
    bool active;
    bool has_active;
    char instruction[COMPANION_LINE_MAX * 2U];
    char distance[32];
    char eta[COMPANION_TEXT_MAX];
} nav_parse_t;

static bool copy_optional_text(const flat_json_value_t *v, char *out,
                               size_t size) {
    if (v->type == FLAT_JSON_NULL) {
        out[0] = '\0';
        return true;
    }
    if (v->type != FLAT_JSON_STRING) return false;
    companion_display_text(v->string, out, size);
    return true;
}

static bool nav_member(void *context, const char *key,
                       const flat_json_value_t *v) {
    nav_parse_t *p = context;
    if (strcmp(key, "active") == 0) {
        if (v->type != FLAT_JSON_BOOL) return false;
        p->active = v->boolean;
        p->has_active = true;
        return true;
    }
    if (strcmp(key, "instruction") == 0) {
        return copy_optional_text(v, p->instruction, sizeof(p->instruction));
    }
    if (strcmp(key, "distance") == 0) {
        return copy_optional_text(v, p->distance, sizeof(p->distance));
    }
    if (strcmp(key, "eta") == 0) {
        return copy_optional_text(v, p->eta, sizeof(p->eta));
    }
    return true; // maneuver_icon_id et membres futurs ignorés
}

static void copy_truncated(char *out, size_t size, const char *src,
                           size_t length) {
    if (size == 0) return;
    if (length >= size) length = size - 1U;
    memcpy(out, src, length);
    out[length] = '\0';
    // Retire l'espace final laissé par une coupure.
    while (length > 0 && out[length - 1U] == ' ') out[--length] = '\0';
}

// « TOURNEZ A DROITE SUR RUE DES LILAS » -> manœuvre + voie. Les maps
// formulent la voie après « VERS », « SUR », « DANS », « ONTO », « TOWARD ».
static void split_instruction(const char *text, companion_nav_t *nav) {
    static const char *const separators[] = {
        " VERS ", " SUR ", " DANS ", " EN DIRECTION DE ", " ONTO ",
        " TOWARD ", " TOWARDS ", " ON ",
    };
    const char *cut = NULL;
    size_t cut_len = 0;
    for (size_t i = 0; i < sizeof(separators) / sizeof(separators[0]); i++) {
        const char *found = strstr(text, separators[i]);
        if (found != NULL && (cut == NULL || found < cut)) {
            cut = found;
            cut_len = strlen(separators[i]);
        }
    }
    if (cut == NULL || cut == text) {
        copy_truncated(nav->instruction, sizeof(nav->instruction), text,
                       strlen(text));
        nav->street[0] = '\0';
        return;
    }
    copy_truncated(nav->instruction, sizeof(nav->instruction), text,
                   (size_t)(cut - text));
    const char *street = cut + cut_len;
    copy_truncated(nav->street, sizeof(nav->street), street, strlen(street));
}

// « 300 M », « 1,2 KM », « 0.5 MI », « 250 FT » ; sinon texte brut.
static void split_distance(const char *text, companion_nav_t *nav) {
    nav->distance_value[0] = '\0';
    nav->distance_unit[0] = '\0';
    const char *p = text;
    while (*p != '\0' && !(*p >= '0' && *p <= '9')) p++;
    if (*p == '\0') {
        copy_truncated(nav->distance_value, sizeof(nav->distance_value), text,
                       strlen(text));
        return;
    }
    const char *start = p;
    while ((*p >= '0' && *p <= '9') || *p == ',' || *p == '.') p++;
    copy_truncated(nav->distance_value, sizeof(nav->distance_value), start,
                   (size_t)(p - start));
    while (*p == ' ') p++;
    static const char *const units[] = {"KM", "MI", "FT", "YD", "M"};
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); i++) {
        const size_t n = strlen(units[i]);
        if (strncmp(p, units[i], n) == 0 &&
            !(p[n] >= 'A' && p[n] <= 'Z')) {
            // Unités d'affichage en minuscules, comme le reste du kit.
            for (size_t k = 0; k < n && k + 1U < sizeof(nav->distance_unit);
                 k++) {
                nav->distance_unit[k] = (char)(units[i][k] - 'A' + 'a');
                nav->distance_unit[k + 1U] = '\0';
            }
            return;
        }
    }
}

bool companion_parse_nav(const char *json, size_t length,
                         companion_nav_t *out) {
    nav_parse_t p = {0};
    if (out == NULL || !parse(json, length, nav_member, &p) ||
        !p.has_active) {
        return false;
    }
    companion_nav_t nav = {.active = p.active};
    if (p.active) {
        split_instruction(p.instruction, &nav);
        split_distance(p.distance, &nav);
        copy_truncated(nav.eta, sizeof(nav.eta), p.eta, strlen(p.eta));
        nav.maneuver = companion_maneuver_from_text(p.instruction);
    }
    *out = nav;
    return true;
}

typedef struct {
    bool is_nav_key;
    bool has_key;
    companion_key_t key;
} key_parse_t;

static bool key_member(void *context, const char *key,
                       const flat_json_value_t *v) {
    key_parse_t *p = context;
    if (strcmp(key, "type") == 0) {
        p->is_nav_key = v->type == FLAT_JSON_STRING &&
                        strcmp(v->string, "nav_key") == 0;
        return true;
    }
    if (strcmp(key, "key") == 0) {
        static const char *const names[] = {
            [COMPANION_KEY_NEXT] = "next",   [COMPANION_KEY_PREVIOUS] = "previous",
            [COMPANION_KEY_UP] = "up",       [COMPANION_KEY_DOWN] = "down",
            [COMPANION_KEY_LEFT] = "left",   [COMPANION_KEY_RIGHT] = "right",
            [COMPANION_KEY_OK] = "ok",       [COMPANION_KEY_BACK] = "back",
        };
        if (v->type != FLAT_JSON_STRING) return false;
        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
            if (strcmp(v->string, names[i]) == 0) {
                p->key = (companion_key_t)i;
                p->has_key = true;
                return true;
            }
        }
        return false;
    }
    return true;
}

bool companion_parse_key(const char *json, size_t length,
                         companion_key_t *out) {
    key_parse_t p = {0};
    if (out == NULL || !parse(json, length, key_member, &p) ||
        !p.is_nav_key || !p.has_key) {
        return false;
    }
    *out = p.key;
    return true;
}

typedef struct {
    companion_time_t time;
    bool has_epoch;
    bool has_offset;
} time_parse_t;

static bool time_member(void *context, const char *key,
                        const flat_json_value_t *v) {
    time_parse_t *p = context;
    if (strcmp(key, "epoch_ms") == 0) {
        if (!is_integer(v) || v->number < 0) return false;
        p->time.epoch_ms = (int64_t)v->number;
        p->has_epoch = true;
    } else if (strcmp(key, "tz_offset_min") == 0) {
        // Fuseaux réels : UTC-12:00 .. UTC+14:00, par quarts d'heure.
        if (!is_integer(v) || v->number < -720 || v->number > 840 ||
            fmod(v->number, 15.0) != 0.0) {
            return false;
        }
        p->time.utc_offset_min = (int16_t)v->number;
        p->has_offset = true;
    }
    return true;
}

bool companion_parse_time(const char *json, size_t length,
                          companion_time_t *out) {
    time_parse_t p = {0};
    if (out == NULL || !parse(json, length, time_member, &p) ||
        !p.has_epoch || !p.has_offset) {
        return false;
    }
    *out = p.time;
    return true;
}

// ── Manœuvre ────────────────────────────────────────────────────────────────
static bool contains(const char *text, const char *word) {
    return strstr(text, word) != NULL;
}

companion_maneuver_t companion_maneuver_from_text(const char *text) {
    if (text == NULL || text[0] == '\0') return COMPANION_MANEUVER_UNKNOWN;
    if (contains(text, "DEMI-TOUR") || contains(text, "DEMI TOUR") ||
        contains(text, "U-TURN") || contains(text, "U TURN")) {
        return COMPANION_MANEUVER_UTURN;
    }
    if (contains(text, "ROND-POINT") || contains(text, "ROND POINT") ||
        contains(text, "ROUNDABOUT") || contains(text, "GIRATOIRE")) {
        return COMPANION_MANEUVER_ROUNDABOUT;
    }
    if (contains(text, "ARRIV") || contains(text, "DESTINATION")) {
        return COMPANION_MANEUVER_ARRIVE;
    }
    const bool left = contains(text, "GAUCHE") || contains(text, "LEFT");
    const bool right = contains(text, "DROITE") || contains(text, "RIGHT");
    const bool slight = contains(text, "LEGEREMENT") || contains(text, "SLIGHT") ||
                        contains(text, "SERREZ") || contains(text, "KEEP") ||
                        contains(text, "RESTEZ") || contains(text, "BIFURQU");
    const bool sharp = contains(text, "FRANCHEMENT") || contains(text, "SHARP") ||
                       contains(text, "SERRE A");
    // « Tout droit » contient « DROIT » mais pas « DROITE ».
    if (left && !right) {
        if (sharp) return COMPANION_MANEUVER_SHARP_LEFT;
        return slight ? COMPANION_MANEUVER_SLIGHT_LEFT : COMPANION_MANEUVER_LEFT;
    }
    if (right && !left) {
        if (sharp) return COMPANION_MANEUVER_SHARP_RIGHT;
        return slight ? COMPANION_MANEUVER_SLIGHT_RIGHT : COMPANION_MANEUVER_RIGHT;
    }
    if (contains(text, "TOUT DROIT") || contains(text, "STRAIGHT") ||
        contains(text, "CONTINUE") || contains(text, "CONTINUEZ") ||
        contains(text, "HEAD ") || contains(text, "DIRIGEZ")) {
        return COMPANION_MANEUVER_STRAIGHT;
    }
    return COMPANION_MANEUVER_UNKNOWN;
}

int64_t companion_media_position_at(const companion_media_t *media,
                                    uint32_t received_ms, uint32_t now_ms) {
    if (media == NULL || media->position_ms < 0) return -1;
    int64_t position = media->position_ms;
    if (media->state == COMPANION_PLAYBACK_PLAYING) {
        position += (int64_t)(uint32_t)(now_ms - received_ms);
    }
    if (media->duration_ms > 0 && position > media->duration_ms) {
        position = media->duration_ms;
    }
    return position;
}
