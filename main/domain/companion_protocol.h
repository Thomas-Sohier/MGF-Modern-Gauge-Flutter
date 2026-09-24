#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Contrat du service GATT de l'application Android « rover-mems-ecu-companion »
// (service 7f3a0001-…). Le téléphone écrit du JSON UTF-8 ; la jauge le décode
// ici, sans dépendance ESP-IDF, puis l'affiche. Les textes sont stockés déjà
// normalisés pour la police Michroma (capitales ASCII, accents repliés).

#define COMPANION_TEXT_MAX 64U
#define COMPANION_LINE_MAX 96U
// Android plafonne une valeur d'attribut GATT à 512 octets.
#define COMPANION_PAYLOAD_MAX 512U

typedef enum {
    COMPANION_PLAYBACK_STOPPED = 0,
    COMPANION_PLAYBACK_PLAYING,
    COMPANION_PLAYBACK_PAUSED,
} companion_playback_t;

typedef struct {
    char title[COMPANION_TEXT_MAX];
    char artist[COMPANION_TEXT_MAX];
    companion_playback_t state;
    int64_t position_ms; // -1 : inconnue
    int64_t duration_ms; // <= 0 : inconnue
} companion_media_t;

typedef enum {
    COMPANION_MANEUVER_UNKNOWN = 0,
    COMPANION_MANEUVER_STRAIGHT,
    COMPANION_MANEUVER_SLIGHT_LEFT,
    COMPANION_MANEUVER_LEFT,
    COMPANION_MANEUVER_SHARP_LEFT,
    COMPANION_MANEUVER_SLIGHT_RIGHT,
    COMPANION_MANEUVER_RIGHT,
    COMPANION_MANEUVER_SHARP_RIGHT,
    COMPANION_MANEUVER_UTURN,
    COMPANION_MANEUVER_ROUNDABOUT,
    COMPANION_MANEUVER_ARRIVE,
} companion_maneuver_t;

typedef struct {
    bool active;
    // Consigne découpée : manœuvre (« TOURNER A DROITE ») et voie (« RUE X »).
    char instruction[COMPANION_LINE_MAX];
    char street[COMPANION_LINE_MAX];
    // Distance découpée en valeur + unité (« 300 » « M », « 1,2 » « KM ») ;
    // sans nombre reconnu, le texte entier va dans distance_value.
    char distance_value[16];
    char distance_unit[8];
    char eta[COMPANION_TEXT_MAX];
    companion_maneuver_t maneuver;
} companion_nav_t;

typedef enum {
    COMPANION_KEY_NEXT = 0,
    COMPANION_KEY_PREVIOUS,
    COMPANION_KEY_UP,
    COMPANION_KEY_DOWN,
    COMPANION_KEY_LEFT,
    COMPANION_KEY_RIGHT,
    COMPANION_KEY_OK,
    COMPANION_KEY_BACK,
} companion_key_t;

typedef struct {
    int64_t epoch_ms;       // UTC
    int16_t utc_offset_min; // fuseau + heure d'été du téléphone
} companion_time_t;

// Décodeurs : false si le JSON est invalide ou incomplet. `out` n'est
// modifié qu'en cas de succès.
bool companion_parse_media(const char *json, size_t length,
                           companion_media_t *out);
bool companion_parse_nav(const char *json, size_t length, companion_nav_t *out);
bool companion_parse_key(const char *json, size_t length, companion_key_t *out);
bool companion_parse_time(const char *json, size_t length,
                          companion_time_t *out);

// Texte affichable par Michroma : capitales ASCII, accents latins repliés
// (É -> E, Œ -> OE), ponctuation typographique simplifiée, autres symboles
// (emoji…) retirés, espaces fusionnés. Toujours terminé par NUL.
void companion_display_text(const char *utf8, char *out, size_t out_size);

// Heuristique FR/EN sur une consigne déjà normalisée (« TOURNEZ A DROITE »).
companion_maneuver_t companion_maneuver_from_text(const char *instruction);

// Position courante extrapolée : la position n'est transmise qu'aux
// changements, la jauge avance l'horloge localement pendant la lecture.
int64_t companion_media_position_at(const companion_media_t *media,
                                    uint32_t received_ms, uint32_t now_ms);
