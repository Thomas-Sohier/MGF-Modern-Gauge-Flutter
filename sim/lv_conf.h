// Configuration LVGL minimale pour le simulateur hôte (génération du golden).
// Seuls les réglages nécessaires au rendu offscreen de l'écran RPM sont posés ;
// lv_conf_internal.h fournit les valeurs par défaut pour le reste.
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

// Couleur 32 bits pour un rendu et un snapshot ARGB8888 propres sur l'hôte.
#define LV_COLOR_DEPTH 32

// malloc/string/sprintf de la libc : le buffer de snapshot (≈2,4 Mo en
// ARGB8888) dépasse le pool builtin par défaut.
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

#define LV_USE_OS LV_OS_NONE

// Boot screen vectoriel (LVGL 9).
#define LV_USE_VECTOR_GRAPHIC 1
#define LV_USE_MATRIX 1
#define LV_USE_FLOAT 1

// Snapshot d'objet -> buffer image (utilisé pour exporter le golden).
#define LV_USE_SNAPSHOT 1

// Rendu de la police Michroma à la volée.
#define LV_USE_TINY_TTF 1
#define LV_TINY_TTF_FILE_SUPPORT 0

// Polices utilisées par l'écran RPM (mêmes tailles qu'en cible).
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#endif // LV_CONF_H
