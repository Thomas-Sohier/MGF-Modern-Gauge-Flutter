#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Pochette du compagnon : contrôle de transfert (caractéristique …0003) et
// réassemblage des chunks binaires (…0004), purs et testables sur hôte.
//
// Le téléphone annonce d'abord un JSON plat :
//   {"art_id":"…", "total_bytes":N, "chunk_count":M}
// puis écrit les chunks en write-without-response, chacun préfixé par son
// index big-endian sur 2 octets. L'ordre est celui de la file GATT Android
// (séquentiel), mais le réassemblage tolère un léger désordre une fois le
// chunk 0 reçu (il fixe alors la taille des chunks pleins).

#define COMPANION_ART_ID_MAX 48U
// Une pochette JPEG 480x480 q80 tient largement sous cette borne ; elle évite
// qu'un contrôle menteur ne fasse réserver une région PSRAM arbitraire.
#define COMPANION_ART_MAX_BYTES (256U * 1024U)
// 2048 chunks couvrent 256 Kio dès ~128 octets utiles par chunk (MTU ≥ 133),
// très en deçà du MTU négocié habituel (~517).
#define COMPANION_ART_MAX_CHUNKS 2048U
#define COMPANION_ART_BITS ((COMPANION_ART_MAX_CHUNKS + 7U) / 8U)

typedef struct {
    char art_id[COMPANION_ART_ID_MAX];
    size_t total_bytes;
    uint16_t chunk_count;
} companion_art_control_t;

// JSON invalide, membre manquant ou valeurs hors bornes -> false ; `out` n'est
// modifié qu'en cas de succès.
bool companion_parse_art_control(const char *json, size_t length,
                                 companion_art_control_t *out);

// Réassemblage sans allocation propre : `buffer` (capacité `capacity`) est
// prêté par l'appelant et reçoit les octets. Le bitmap de réception est
// embarqué dans la structure (256 octets).
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t total_bytes;
    uint16_t chunk_count;
    size_t chunk_size; // taille d'un chunk plein, fixée par le chunk 0
    size_t bytes_received;
    uint16_t received_count;
    bool active;
    bool chunk0_seen;
    uint8_t received[COMPANION_ART_BITS];
} companion_art_reassembler_t;

void companion_art_reassembler_init(companion_art_reassembler_t *r,
                                    uint8_t *buffer, size_t capacity);
// Vide l'état sans toucher au tampon ni à sa capacité.
void companion_art_reassembler_reset(companion_art_reassembler_t *r);
// Prépare un transfert ; false si le contrôle est incohérent ou dépasse la
// capacité du tampon. L'état précédent est toujours abandonné.
bool companion_art_reassembler_begin(companion_art_reassembler_t *r,
                                     const companion_art_control_t *control);
// Ajoute un chunk brut (index big-endian 2 octets + données). false sur toute
// violation : index hors bornes, doublon, chunk reçu avant le 0, trou, taille
// incohérente ou dépassement. L'appelant doit alors réinitialiser le transfert.
bool companion_art_reassembler_push(companion_art_reassembler_t *r,
                                    const uint8_t *chunk, size_t length);
// Tous les chunks reçus et le compte d'octets exact.
bool companion_art_reassembler_complete(const companion_art_reassembler_t *r);

// Pochette complète prête à décoder. Le tampon `data` est la propriété du
// porteur : le réassembleur le prête, l'appelant le transfère (worker ou
// libération explicite).
typedef struct {
    uint8_t *data;
    size_t length;
} companion_art_jpeg_t;
