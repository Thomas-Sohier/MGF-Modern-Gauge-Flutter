#pragma once

#include <stdbool.h>

#include "domain/companion_protocol.h"
#include "host/ble_gatt.h"

// Service GATT de l'application Android compagnon (7f3a0001-…), repris du
// boîtier Linux/Go qu'elle pilotait auparavant. Le téléphone écrit ; la jauge
// décode dans la tâche NimBLE et dépose le dernier état dans une boîte aux
// lettres consommée par la tâche LVGL (aucun appel LVGL ici).
//
// Pochette et icône de manœuvre (images) sont acceptées puis ignorées :
// l'application les exige pour déclarer le lien prêt, l'écran ambre n'en a
// pas l'usage. Idem pour les alertes.

// Tableau de services terminé par {0}, à enregistrer avant le démarrage.
const struct ble_gatt_svc_def *companion_gatt_services(void);
const ble_uuid_t *companion_gatt_service_uuid(void);

// Oublie les messages en attente (nouvelle connexion).
void companion_gatt_reset(void);

// Consommation depuis la tâche LVGL : dernier état reçu, une seule fois.
bool companion_gatt_take_media(companion_media_t *out);
bool companion_gatt_take_nav(companion_nav_t *out);
bool companion_gatt_take_time(companion_time_t *out);
// File courte des touches de la télécommande (ordre conservé).
bool companion_gatt_take_key(companion_key_t *out);
