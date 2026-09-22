#pragma once

#include <stdbool.h>
#include <stdint.h>

#define APP_SETTINGS_SCHEMA_VERSION 1U
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
    app_settings_page_t selected_page;
    app_settings_theme_t theme;
    app_settings_units_t units;
} app_settings_t;

void app_settings_defaults(app_settings_t *settings);
bool app_settings_is_valid(const app_settings_t *settings);
bool app_settings_equal(const app_settings_t *left,
                        const app_settings_t *right);
