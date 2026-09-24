#pragma once

#include <stdbool.h>
#include <stddef.h>

// Lecteur minimal d'objets JSON « plats » ({"clé": valeur, ...}) tels que
// l'application compagnon les émet : chaînes, nombres, booléens et null.
// Aucun objet ni tableau imbriqué n'est accepté. Pur C, sans allocation :
// les chaînes décodées sont copiées dans un tampon fourni par l'appelant.
typedef enum {
    FLAT_JSON_NULL = 0,
    FLAT_JSON_STRING,
    FLAT_JSON_NUMBER,
    FLAT_JSON_BOOL,
} flat_json_type_t;

typedef struct {
    flat_json_type_t type;
    // STRING : texte UTF-8 décodé (échappements résolus), tronqué à la
    // capacité du tampon sur une frontière de caractère.
    const char *string;
    double number;
    bool boolean;
} flat_json_value_t;

// Appelé pour chaque membre, dans l'ordre du document. Retourner false
// interrompt la lecture (flat_json_parse retourne alors false).
typedef bool (*flat_json_member_cb_t)(void *context, const char *key,
                                      const flat_json_value_t *value);

// `scratch` reçoit la clé puis la valeur texte courante ; 2 x 256 octets
// suffisent aux messages du compagnon. Retourne false si le document est
// invalide, contient une structure imbriquée ou si le rappel refuse.
bool flat_json_parse(const char *json, size_t length, char *scratch,
                     size_t scratch_size, flat_json_member_cb_t callback,
                     void *context);
