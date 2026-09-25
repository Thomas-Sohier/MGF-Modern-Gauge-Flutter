#pragma once

#include "lvgl.h"
#include "domain/ecu_data.h"
#include "domain/rtc.h"

// Horloge analogique ambre pour le panneau rond 480x480. Le cadran et les
// aiguilles sont dessinés par des primitives LVGL, sans image ni buffer dédié.
// Le DS3231 et le repli système stockent de l'UTC ; l'heure affichée est
// l'heure légale, UTC + décalage fourni par le téléphone (fuseau + été).
typedef struct clock_screen_s clock_screen_t;

// `parent` doit rester vivant jusqu'à clock_screen_destroy(). Les fonctions
// manipulent LVGL dans son thread (ou sous son verrou).
clock_screen_t *clock_screen_create(lv_obj_t *parent);
void clock_screen_set_rtc(clock_screen_t *screen, rtc_t *rtc);
void clock_screen_set_utc_offset(clock_screen_t *screen, int16_t minutes);
void clock_screen_update(clock_screen_t *screen, const ecu_data_t *data);

// Applique une heure légale déjà résolue sans relire le RTC ni l'horloge
// système, pour le simulateur et les tests. `valid` est forcé à faux si
// `hour`/`minute` sortent des bornes 0..23 / 0..59. Un résultat invalide
// masque les aiguilles et le pivot et affiche le message central
// « HEURE / NON SYNCHRONISEE » ; le cadran reste dessiné. Le passage
// valide -> invalide efface l'heure même si hour/minute sont identiques.
void clock_screen_set_time(clock_screen_t *screen, int hour, int minute,
                           bool valid);
void clock_screen_destroy(clock_screen_t *screen);
