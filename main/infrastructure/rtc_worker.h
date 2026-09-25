#pragma once

#include <stdbool.h>

#include "domain/rtc.h"

// Worker I2C de la RTC (DS3231) exécuté hors de la boucle LVGL.
//
// Le RTC est un périphérique I2C partagé : une transaction bloquante ne doit
// jamais figer le rendu. Ce worker possède une tâche FreeRTOS unique qui :
//   - lit la RTC au démarrage (hors LVGL) et initialise l'horloge système en
//     UTC si la valeur est valide ; en cas d'échec, réessaie toutes les 5 s ;
//   - une fois synchronisée, relit la RTC toutes les 60 s pour recaler
//     l'horloge système, sans jamais la reculer si la RTC échoue ;
//   - applique les commandes d'heure du téléphone (écriture RTC) hors LVGL.
//     Une écriture en échec est réessayée toutes les 5 s en dernier-gagne
//     (jamais en boucle active).
//
// PRIORITÉ TÉLÉPHONE (politique de session) :
//   - le worker n'écrit JAMAIS l'horloge système pour une commande téléphone ;
//     c'est l'appelant qui fait `rtc_worker_submit` PUIS `settimeofday`
//     (l'acquittement en file précède la mise à l'heure).
//   - `rtc_worker_submit` publie `phone_seen` et met la commande en file SOUS
//     LE MÊME VERROU. Une fois `phone_seen` vrai, le recalage périodique depuis
//     la RTC s'arrête pour toute la session (plus de lecture périodique) et
//     l'horloge système n'est plus jamais touchée par le worker : une heure
//     téléphone plus récente ne peut donc pas être écrasée par la RTC.
//   - la déduplication d'un renvoi identique se fait dans la tâche, au moment
//     de traiter la commande, contre le dernier UTC brut réellement écrit avec
//     succès (état privé à la tâche, jamais consulté par le submit).
//
// API (contrat) :
//   - `borrowed == NULL` (RTC optionnelle absente) : rtc_worker_start retourne
//     NULL et rtc_worker_submit refuse la commande. Le worker n'est jamais
//     propriétaire de la RTC et ne la détruit pas.
//   - rtc_worker_submit est appelable depuis n'importe quelle tâche, valide
//     l'UTC et publie la commande en « dernier-gagne » (file bornée à 1, pas de
//     file d'attente qui accumule).
//   - rtc_worker_submit == true signifie ACCEPTÉ / MIS EN FILE, pas écrit en
//     I2C : c'est le sens de RTC_OK pour ble_datetime_set.
typedef struct rtc_worker_s rtc_worker_t;

// Démarre le worker sur une RTC empruntée. NULL si `borrowed == NULL` ou si
// les ressources FreeRTOS sont indisponibles.
rtc_worker_t *rtc_worker_start(rtc_t *borrowed);

// Valide, déduplique et met en file une heure UTC (true = acceptée en file).
bool rtc_worker_submit(rtc_worker_t *worker, const rtc_datetime_t *utc);

// Demande l'arrêt, attend la fin réelle de la tâche puis libère le worker.
// À appeler avant rtc_destroy() et board_display_stop(). Idempotent sur NULL.
void rtc_worker_stop(rtc_worker_t *worker);
