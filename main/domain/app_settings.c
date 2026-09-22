#include "domain/app_settings.h"

#include <stddef.h>

void app_settings_defaults(app_settings_t *settings) {
    if (settings == NULL) return;

    *settings = (app_settings_t){
        .brightness_percent = APP_SETTINGS_DEFAULT_BRIGHTNESS_PERCENT,
        .selected_page = APP_SETTINGS_PAGE_RPM,
        .theme = APP_SETTINGS_THEME_AMBER,
        .units = APP_SETTINGS_UNITS_METRIC,
    };
}

bool app_settings_is_valid(const app_settings_t *settings) {
    return settings != NULL &&
           settings->brightness_percent <= 100U &&
           settings->selected_page < APP_SETTINGS_PAGE_COUNT &&
           settings->theme < APP_SETTINGS_THEME_COUNT &&
           settings->units < APP_SETTINGS_UNITS_COUNT;
}

bool app_settings_equal(const app_settings_t *left,
                        const app_settings_t *right) {
    return left != NULL && right != NULL &&
           left->brightness_percent == right->brightness_percent &&
           left->selected_page == right->selected_page &&
           left->theme == right->theme &&
           left->units == right->units;
}
