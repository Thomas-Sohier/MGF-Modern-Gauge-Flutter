#include "domain/flat_json.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *cursor;
    const char *end;
} reader_t;

typedef struct {
    char *data;
    size_t capacity; // octets utiles, hors terminateur
    size_t length;
    bool full;       // une fois tronqué, plus rien n'est ajouté
} text_buffer_t;

static void skip_space(reader_t *r) {
    while (r->cursor < r->end &&
           (*r->cursor == ' ' || *r->cursor == '\t' || *r->cursor == '\n' ||
            *r->cursor == '\r')) {
        r->cursor++;
    }
}

static bool consume(reader_t *r, char expected) {
    skip_space(r);
    if (r->cursor >= r->end || *r->cursor != expected) return false;
    r->cursor++;
    return true;
}

// Ajoute un caractère UTF-8 complet, ou rien s'il ne tient pas : la chaîne
// tronquée reste valide.
static void append_bytes(text_buffer_t *out, const char *bytes, size_t count) {
    if (out->full) return;
    if (out->length + count > out->capacity) {
        out->full = true;
        return;
    }
    memcpy(out->data + out->length, bytes, count);
    out->length += count;
}

static void append_codepoint(text_buffer_t *out, uint32_t cp) {
    char bytes[4];
    size_t count;
    if (cp < 0x80u) {
        bytes[0] = (char)cp;
        count = 1;
    } else if (cp < 0x800u) {
        bytes[0] = (char)(0xC0u | (cp >> 6));
        bytes[1] = (char)(0x80u | (cp & 0x3Fu));
        count = 2;
    } else if (cp < 0x10000u) {
        bytes[0] = (char)(0xE0u | (cp >> 12));
        bytes[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        bytes[2] = (char)(0x80u | (cp & 0x3Fu));
        count = 3;
    } else {
        bytes[0] = (char)(0xF0u | (cp >> 18));
        bytes[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
        bytes[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        bytes[3] = (char)(0x80u | (cp & 0x3Fu));
        count = 4;
    }
    append_bytes(out, bytes, count);
}

static bool read_hex4(reader_t *r, uint32_t *out) {
    if (r->end - r->cursor < 4) return false;
    uint32_t value = 0;
    for (int i = 0; i < 4; i++) {
        const char c = *r->cursor++;
        value <<= 4;
        if (c >= '0' && c <= '9') value |= (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') value |= (uint32_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') value |= (uint32_t)(c - 'A' + 10);
        else return false;
    }
    *out = value;
    return true;
}

// Longueur d'une séquence UTF-8 d'après son octet de tête (0 : invalide).
static size_t utf8_sequence_length(unsigned char lead) {
    if (lead < 0x80u) return 1;
    if ((lead & 0xE0u) == 0xC0u) return 2;
    if ((lead & 0xF0u) == 0xE0u) return 3;
    if ((lead & 0xF8u) == 0xF0u) return 4;
    return 0;
}

static bool read_string(reader_t *r, text_buffer_t *out) {
    if (!consume(r, '"')) return false;
    out->length = 0;
    out->full = false;
    while (r->cursor < r->end) {
        const unsigned char c = (unsigned char)*r->cursor;
        if (c == '"') {
            r->cursor++;
            out->data[out->length] = '\0';
            return true;
        }
        if (c < 0x20u) return false; // caractère de contrôle brut interdit
        if (c != '\\') {
            const size_t count = utf8_sequence_length(c);
            if (count == 0 || (size_t)(r->end - r->cursor) < count) return false;
            for (size_t i = 1; i < count; i++) {
                if ((((unsigned char)r->cursor[i]) & 0xC0u) != 0x80u) {
                    return false;
                }
            }
            append_bytes(out, r->cursor, count);
            r->cursor += count;
            continue;
        }
        r->cursor++;
        if (r->cursor >= r->end) return false;
        const char escape = *r->cursor++;
        switch (escape) {
        case '"': append_bytes(out, "\"", 1); break;
        case '\\': append_bytes(out, "\\", 1); break;
        case '/': append_bytes(out, "/", 1); break;
        case 'b': append_bytes(out, "\b", 1); break;
        case 'f': append_bytes(out, "\f", 1); break;
        case 'n': append_bytes(out, "\n", 1); break;
        case 'r': append_bytes(out, "\r", 1); break;
        case 't': append_bytes(out, "\t", 1); break;
        case 'u': {
            uint32_t cp;
            if (!read_hex4(r, &cp)) return false;
            if (cp >= 0xD800u && cp <= 0xDBFFu) {
                uint32_t low;
                if (r->end - r->cursor < 6 || r->cursor[0] != '\\' ||
                    r->cursor[1] != 'u') {
                    return false;
                }
                r->cursor += 2;
                if (!read_hex4(r, &low) || low < 0xDC00u || low > 0xDFFFu) {
                    return false;
                }
                cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
            } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                return false;
            }
            append_codepoint(out, cp);
            break;
        }
        default:
            return false;
        }
    }
    return false;
}

static bool match_literal(reader_t *r, const char *literal) {
    const size_t length = strlen(literal);
    if ((size_t)(r->end - r->cursor) < length ||
        memcmp(r->cursor, literal, length) != 0) {
        return false;
    }
    r->cursor += length;
    return true;
}

static bool read_number(reader_t *r, double *out) {
    // Copie bornée : strtod exige une chaîne terminée.
    char buffer[40];
    size_t length = 0;
    while (r->cursor + length < r->end && length < sizeof(buffer) - 1U) {
        const char c = r->cursor[length];
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
            c == 'e' || c == 'E') {
            length++;
        } else {
            break;
        }
    }
    if (length == 0) return false;
    memcpy(buffer, r->cursor, length);
    buffer[length] = '\0';
    char *parsed_end = NULL;
    const double value = strtod(buffer, &parsed_end);
    if (parsed_end != buffer + length) return false;
    r->cursor += length;
    *out = value;
    return true;
}

bool flat_json_parse(const char *json, size_t length, char *scratch,
                     size_t scratch_size, flat_json_member_cb_t callback,
                     void *context) {
    if (json == NULL || scratch == NULL || scratch_size < 4U ||
        callback == NULL) {
        return false;
    }
    const size_t half = scratch_size / 2U;
    text_buffer_t key = {.data = scratch, .capacity = half - 1U};
    text_buffer_t text = {.data = scratch + half,
                          .capacity = scratch_size - half - 1U};
    reader_t r = {.cursor = json, .end = json + length};

    if (!consume(&r, '{')) return false;
    skip_space(&r);
    if (r.cursor < r.end && *r.cursor == '}') {
        r.cursor++;
    } else {
        for (;;) {
            if (!read_string(&r, &key) || !consume(&r, ':')) return false;
            skip_space(&r);
            if (r.cursor >= r.end) return false;

            flat_json_value_t value = {0};
            const char c = *r.cursor;
            if (c == '"') {
                if (!read_string(&r, &text)) return false;
                value.type = FLAT_JSON_STRING;
                value.string = text.data;
            } else if (match_literal(&r, "true")) {
                value.type = FLAT_JSON_BOOL;
                value.boolean = true;
            } else if (match_literal(&r, "false")) {
                value.type = FLAT_JSON_BOOL;
            } else if (match_literal(&r, "null")) {
                value.type = FLAT_JSON_NULL;
            } else if (c == '-' || (c >= '0' && c <= '9')) {
                if (!read_number(&r, &value.number)) return false;
                value.type = FLAT_JSON_NUMBER;
            } else {
                return false; // objet ou tableau imbriqué, ou jeton inconnu
            }
            if (!callback(context, key.data, &value)) return false;

            skip_space(&r);
            if (r.cursor < r.end && *r.cursor == ',') {
                r.cursor++;
                continue;
            }
            if (!consume(&r, '}')) return false;
            break;
        }
    }
    skip_space(&r);
    // Tolère un NUL final (certaines piles ajoutent un terminateur).
    while (r.cursor < r.end && *r.cursor == '\0') r.cursor++;
    return r.cursor == r.end;
}
