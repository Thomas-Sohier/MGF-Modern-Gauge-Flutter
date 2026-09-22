#pragma once

#include <stdbool.h>
#include <stdint.h>

// Date/heure civile représentable directement par un DS3231. Les années sont
// volontairement limitées à 2000..2099 (plage sans ambiguïté du composant).
//
// Contrat de l'application : cette valeur est en UTC. Le DS3231 ne contient
// ni fuseau ni règle d'heure d'été ; une conversion locale doit donc être
// effectuée avant l'affichage ou par le client qui fournit la synchronisation.
// Tant qu'aucun fuseau n'est configuré, aucune date locale ne doit être écrite
// comme si elle était UTC.
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday; // 1 = dimanche, ... 7 = samedi
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} rtc_datetime_t;

typedef enum {
    RTC_OK = 0,
    RTC_ERR_INVALID_ARGUMENT,
    RTC_ERR_IO,
    RTC_ERR_OSCILLATOR_STOPPED,
    RTC_ERR_INVALID_TIME,
} rtc_result_t;

typedef struct rtc_s rtc_t;

// Contrat commun aux RTC : l'objet est synchrone et non propriétaire du bus.
// Les appels sont courts, mais peuvent attendre une transaction I2C.
rtc_result_t rtc_probe(rtc_t *rtc);
rtc_result_t rtc_read(rtc_t *rtc, rtc_datetime_t *out);
rtc_result_t rtc_set(rtc_t *rtc, const rtc_datetime_t *date_time);
void rtc_destroy(rtc_t *rtc);
const char *rtc_result_name(rtc_result_t result);

// Validation indépendante de toute plateforme, utilisable avant une écriture.
bool rtc_datetime_is_valid(const rtc_datetime_t *date_time);
