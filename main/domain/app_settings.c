#include "domain/app_settings.h"

#include <stddef.h>

void app_settings_defaults(app_settings_t *settings) {
    if (settings == NULL) return;

    *settings = (app_settings_t){
        .brightness_percent = APP_SETTINGS_DEFAULT_BRIGHTNESS_PERCENT,
        .selected_page = APP_SETTINGS_PAGE_RPM,
        .startup_page = APP_SETTINGS_STARTUP_LAST_PAGE,
        .theme = APP_SETTINGS_THEME_AMBER,
        .units = APP_SETTINGS_UNITS_METRIC,
    };
}

bool app_settings_is_valid(const app_settings_t *settings) {
    return settings != NULL && settings->brightness_percent <= 100U &&
           settings->selected_page >= 0 &&
           settings->selected_page < APP_SETTINGS_PAGE_COUNT &&
           settings->startup_page >= 0 &&
           settings->startup_page <= APP_SETTINGS_STARTUP_LAST_PAGE &&
           settings->theme >= 0 && settings->theme < APP_SETTINGS_THEME_COUNT &&
           settings->units >= 0 && settings->units < APP_SETTINGS_UNITS_COUNT &&
           settings->utc_offset_minutes >= APP_SETTINGS_UTC_OFFSET_MIN &&
           settings->utc_offset_minutes <= APP_SETTINGS_UTC_OFFSET_MAX &&
           settings->utc_offset_minutes % 15 == 0;
}

app_settings_page_t app_settings_boot_page(const app_settings_t *settings) {
    if (settings == NULL) return APP_SETTINGS_PAGE_RPM;
    return settings->startup_page == APP_SETTINGS_STARTUP_LAST_PAGE
               ? settings->selected_page
               : settings->startup_page;
}

void app_settings_merge_ble_v1(const app_settings_t *current,
                               app_settings_t *incoming) {
    if (current == NULL || incoming == NULL) return;
    incoming->startup_page = current->startup_page;
    incoming->utc_offset_minutes = current->utc_offset_minutes;
}

bool app_settings_equal(const app_settings_t *left,
                        const app_settings_t *right) {
    return left != NULL && right != NULL &&
           left->brightness_percent == right->brightness_percent &&
           left->selected_page == right->selected_page &&
           left->startup_page == right->startup_page &&
           left->theme == right->theme && left->units == right->units &&
           left->utc_offset_minutes == right->utc_offset_minutes;
}
