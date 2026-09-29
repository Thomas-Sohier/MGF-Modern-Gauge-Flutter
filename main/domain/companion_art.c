#include "domain/companion_art.h"

#include <math.h>
#include <string.h>

#include "domain/flat_json.h"

static bool json_is_integer(const flat_json_value_t *v) {
    return v->type == FLAT_JSON_NUMBER && isfinite(v->number) &&
           fabs(v->number) < 9.0e15 && floor(v->number) == v->number;
}

typedef struct {
    companion_art_control_t control;
    bool has_id;
    bool has_total;
    bool has_chunks;
} art_control_parse_t;

static bool art_control_member(void *context, const char *key,
                               const flat_json_value_t *v) {
    art_control_parse_t *p = context;
    if (strcmp(key, "art_id") == 0) {
        if (v->type != FLAT_JSON_STRING) return false;
        const size_t length = strlen(v->string);
        if (length == 0 || length >= sizeof(p->control.art_id)) return false;
        memcpy(p->control.art_id, v->string, length + 1U);
        p->has_id = true;
        return true;
    }
    if (strcmp(key, "total_bytes") == 0) {
        if (!json_is_integer(v) || v->number < 1.0 ||
            v->number > (double)COMPANION_ART_MAX_BYTES) {
            return false;
        }
        p->control.total_bytes = (size_t)v->number;
        p->has_total = true;
        return true;
    }
    if (strcmp(key, "chunk_count") == 0) {
        if (!json_is_integer(v) || v->number < 1.0 ||
            v->number > (double)COMPANION_ART_MAX_CHUNKS) {
            return false;
        }
        p->control.chunk_count = (uint16_t)v->number;
        p->has_chunks = true;
        return true;
    }
    return true;
}

bool companion_parse_art_control(const char *json, size_t length,
                                 companion_art_control_t *out) {
    if (json == NULL || length == 0 || length > COMPANION_ART_MAX_BYTES) {
        return false;
    }
    art_control_parse_t p = {0};
    char scratch[256];
    if (!flat_json_parse(json, length, scratch, sizeof(scratch),
                         art_control_member, &p)) {
        return false;
    }
    if (!p.has_id || !p.has_total || !p.has_chunks) return false;
    // Chaque chunk porte au moins un octet utile (en plus de son index).
    if (p.control.chunk_count > p.control.total_bytes) return false;
    if (out != NULL) *out = p.control;
    return true;
}

void companion_art_reassembler_init(companion_art_reassembler_t *r,
                                    uint8_t *buffer, size_t capacity) {
    if (r == NULL) return;
    memset(r, 0, sizeof(*r));
    r->buffer = buffer;
    r->capacity = capacity;
}

void companion_art_reassembler_reset(companion_art_reassembler_t *r) {
    if (r == NULL) return;
    r->total_bytes = 0;
    r->chunk_count = 0;
    r->chunk_size = 0;
    r->bytes_received = 0;
    r->received_count = 0;
    r->active = false;
    r->chunk0_seen = false;
    memset(r->received, 0, sizeof(r->received));
}

bool companion_art_reassembler_begin(companion_art_reassembler_t *r,
                                     const companion_art_control_t *control) {
    if (r == NULL || r->buffer == NULL || control == NULL) return false;
    companion_art_reassembler_reset(r);
    if (control->total_bytes == 0 ||
        control->total_bytes > COMPANION_ART_MAX_BYTES ||
        control->chunk_count == 0 ||
        control->chunk_count > COMPANION_ART_MAX_CHUNKS ||
        control->chunk_count > control->total_bytes ||
        control->total_bytes > r->capacity) {
        return false;
    }
    r->total_bytes = control->total_bytes;
    r->chunk_count = control->chunk_count;
    r->active = true;
    return true;
}

static bool bit_get(const uint8_t *bits, size_t index) {
    return (bits[index >> 3] & (uint8_t)(1U << (index & 7U))) != 0;
}

static void bit_set(uint8_t *bits, size_t index) {
    bits[index >> 3] |= (uint8_t)(1U << (index & 7U));
}

bool companion_art_reassembler_push(companion_art_reassembler_t *r,
                                    const uint8_t *chunk, size_t length) {
    if (r == NULL || chunk == NULL || !r->active) return false;
    // Index 2 octets + au moins un octet de données ; 512 octets bornent une
    // écriture GATT.
    if (length < 3U || length > 512U) return false;

    const uint16_t index = (uint16_t)(((uint16_t)chunk[0] << 8) | chunk[1]);
    if (index >= r->chunk_count) return false;
    if (bit_get(r->received, index)) return false; // doublon

    const size_t payload = length - 2U;
    size_t chunk_size = r->chunk_size;
    if (index == 0U) {
        if (r->chunk_count == 1U) {
            // Unique chunk : il doit couvrir exactement tout le fichier.
            if (payload != r->total_bytes) return false;
        } else if (payload >= r->total_bytes) {
            return false; // il doit rester des octets pour les chunks suivants
        }
        chunk_size = payload;
    } else if (!r->chunk0_seen || r->chunk_size == 0U) {
        // Sans le chunk 0, la taille des chunks pleins est inconnue : un chunk
        // ultérieur ne peut pas être placé sans risque de trou.
        return false;
    }

    const size_t offset =
        index == 0U ? 0U : (size_t)index * chunk_size;
    if (offset >= r->total_bytes) return false; // débordement

    const size_t remaining = r->total_bytes - offset;
    const size_t expected =
        remaining < chunk_size ? remaining : chunk_size;
    if (payload != expected) return false;
    if (offset + payload > r->capacity) return false;

    if (index == 0U) {
        r->chunk_size = chunk_size;
        r->chunk0_seen = true;
    }
    memcpy(r->buffer + offset, chunk + 2, payload);
    bit_set(r->received, index);
    r->bytes_received += payload;
    r->received_count++;
    return true;
}

bool companion_art_reassembler_complete(const companion_art_reassembler_t *r) {
    return r != NULL && r->active && r->chunk0_seen &&
           r->received_count == r->chunk_count &&
           r->bytes_received == r->total_bytes;
}
