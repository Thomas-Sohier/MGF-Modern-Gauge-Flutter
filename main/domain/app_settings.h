#pragma once

#include <stdbool.h>
#include <stdint.h>

#define APP_SETTINGS_SCHEMA_VERSION             2U
#define APP_SETTINGS_DEFAULT_BRIGHTNESS_PERCENT 100U

typedef enum {
    APP_SETTINGS_PAGE_CLOCK = 0,
    APP_SETTINGS_PAGE_MUSIC,
    APP_SETTINGS_PAGE_NAVIGATION,
    APP_SETTINGS_PAGE_RPM,
    APP_SETTINGS_PAGE_FAULTS,
    APP_SETTINGS_PAGE_TEMPERATURES,
    APP_SETTINGS_PAGE_INJECTION,
    APP_SETTINGS_PAGE_LAMBDA,
    APP_SETTINGS_PAGE_IGNITION,
    APP_SETTINGS_PAGE_IDLE,
    APP_SETTINGS_PAGE_ADMISSION,
    APP_SETTINGS_PAGE_COUNT,
} app_settings_page_t;

// Valeur de startup_page : rouvrir la dernière page vue (comportement du
// schéma 1). Toute autre valeur < APP_SETTINGS_PAGE_COUNT est une page fixe.
#define APP_SETTINGS_STARTUP_LAST_PAGE                                         \
    ((app_settings_page_t)APP_SETTINGS_PAGE_COUNT)

typedef enum {
    APP_SETTINGS_THEME_AMBER = 0,
    APP_SETTINGS_THEME_COUNT,
} app_settings_theme_t;

typedef enum {
    APP_SETTINGS_UNITS_METRIC = 0,
    APP_SETTINGS_UNITS_IMPERIAL,
    APP_SETTINGS_UNITS_COUNT,
} app_settings_units_t;

typedef struct {
    uint8_t brightness_percent;
    app_settings_page_t selected_page; // dernière page vue
    app_settings_page_t startup_page;  // page fixe ou STARTUP_LAST_PAGE
    app_settings_theme_t theme;
    app_settings_units_t units;
    // Décalage UTC -> heure légale (fuseau + heure d'été), fourni par le
    // téléphone à chaque connexion ; multiple de 15, -720..+840.
    int16_t utc_offset_minutes;
} app_settings_t;

#define APP_SETTINGS_UTC_OFFSET_MIN (-720)
#define APP_SETTINGS_UTC_OFFSET_MAX 840

void app_settings_defaults(app_settings_t *settings);
bool app_settings_is_valid(const app_settings_t *settings);
// Page à afficher au démarrage : la page fixe choisie, sinon la dernière vue.
app_settings_page_t app_settings_boot_page(const app_settings_t *settings);
// Champs absents du protocole BLE de réglages v1 (page de démarrage,
// décalage horaire) : une écriture BLE conserve les valeurs locales.
void app_settings_merge_ble_v1(const app_settings_t *current,
                               app_settings_t *incoming);
bool app_settings_equal(const app_settings_t *left,
                        const app_settings_t *right);
