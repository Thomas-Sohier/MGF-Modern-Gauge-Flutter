#include <assert.h>
#include <stdio.h>

#include "domain/app_settings.h"

static void test_defaults(void) {
    app_settings_t settings = {0};
    app_settings_defaults(&settings);

    assert(settings.brightness_percent == 100U);
    assert(settings.selected_page == APP_SETTINGS_PAGE_RPM);
    assert(settings.theme == APP_SETTINGS_THEME_AMBER);
    assert(settings.units == APP_SETTINGS_UNITS_METRIC);
    assert(app_settings_is_valid(&settings));
}

static void test_validation_boundaries(void) {
    app_settings_t settings;
    app_settings_defaults(&settings);

    settings.brightness_percent = 0;
    assert(app_settings_is_valid(&settings));
    settings.brightness_percent = 100;
    assert(app_settings_is_valid(&settings));

    settings.brightness_percent = 101;
    assert(!app_settings_is_valid(&settings));
    app_settings_defaults(&settings);
    settings.selected_page = APP_SETTINGS_PAGE_COUNT;
    assert(!app_settings_is_valid(&settings));
    app_settings_defaults(&settings);
    settings.theme = APP_SETTINGS_THEME_COUNT;
    assert(!app_settings_is_valid(&settings));
    app_settings_defaults(&settings);
    settings.units = APP_SETTINGS_UNITS_COUNT;
    assert(!app_settings_is_valid(&settings));

    app_settings_defaults(&settings);
    settings.selected_page = (app_settings_page_t)-1;
    assert(!app_settings_is_valid(&settings));
    app_settings_defaults(&settings);
    settings.theme = (app_settings_theme_t)-1;
    assert(!app_settings_is_valid(&settings));
    app_settings_defaults(&settings);
    settings.units = (app_settings_units_t)-1;
    assert(!app_settings_is_valid(&settings));
}

static void test_equality(void) {
    app_settings_t left;
    app_settings_t right;
    app_settings_defaults(&left);
    right = left;

    assert(app_settings_equal(&left, &right));
    right.units = APP_SETTINGS_UNITS_IMPERIAL;
    assert(!app_settings_equal(&left, &right));
    assert(!app_settings_equal(NULL, &right));
}

int main(void) {
    test_defaults();
    test_validation_boundaries();
    test_equality();
    puts("app settings tests: OK");
    return 0;
}
