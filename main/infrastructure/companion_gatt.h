#pragma once

#include <stdbool.h>

#include "domain/companion_art.h"
#include "domain/companion_protocol.h"
#include "host/ble_gatt.h"

// Service GATT de l'application Android compagnon (7f3a0001-…), repris du
// boîtier Linux/Go qu'elle pilotait auparavant. Le téléphone écrit ; la jauge
// décode dans la tâche NimBLE et dépose le dernier état dans une boîte aux
// lettres consommée par la tâche LVGL (aucun appel LVGL ici).
//
// Pochette : le contrôle …0003 ({"art_id","total_bytes","chunk_count"})
// ouvre un réassemblage dans un tampon PSRAM, les chunks …0004 (index
// big-endian 2 octets + données, write-without-response) le remplissent. Une
// pochette complète est publiée telle quelle ; le décodage JPEG est fait par
// le worker hors LVGL. L'icône de manœuvre et les alertes restent ignorées.

// Tableau de services terminé par {0}, à enregistrer avant le démarrage.
const struct ble_gatt_svc_def *companion_gatt_services(void);
const ble_uuid_t *companion_gatt_service_uuid(void);

// Oublie les messages et tout transfert de pochette en cours (nouvelle
// connexion).
void companion_gatt_reset(void);

// Consommation depuis la tâche LVGL : dernier état reçu, une seule fois.
bool companion_gatt_take_media(companion_media_t *out);
bool companion_gatt_take_nav(companion_nav_t *out);
bool companion_gatt_take_time(companion_time_t *out);
// File courte des touches de la télécommande (ordre conservé).
bool companion_gatt_take_key(companion_key_t *out);
// Pochette JPEG complète (dernier-gagnant). En cas de succès, `out->data` est
// alloué en PSRAM et appartient à l'appelant (à transmettre au worker ou à
// libérer avec companion_gatt_free_art). false s'il n'y a rien de prêt.
bool companion_gatt_take_art(companion_art_jpeg_t *out);
// Libère un tampon de pochette non transféré.
void companion_gatt_free_art(uint8_t *data);
