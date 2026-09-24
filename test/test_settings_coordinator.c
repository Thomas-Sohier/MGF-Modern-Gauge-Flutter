#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "app/settings_coordinator.h"

struct save_spy {
    unsigned int calls;
    bool fail;
    app_settings_t saved;
};

static bool save_settings(void *context, const app_settings_t *settings) {
    struct save_spy *spy = context;
    spy->calls++;
    if (spy->fail) return false;
    spy->saved = *settings;
    return true;
}

static settings_coordinator_t *new_coordinator(struct save_spy *spy) {
    app_settings_t settings;
    app_settings_defaults(&settings);
    return settings_coordinator_create(&settings, save_settings, spy);
}

static void test_selection_debounce_and_noop(void) {
    struct save_spy spy = {0};
    settings_coordinator_t *coordinator = new_coordinator(&spy);
    assert(coordinator != NULL);
    assert(!settings_coordinator_is_dirty(coordinator));

    settings_coordinator_page_changed(coordinator, APP_SETTINGS_PAGE_MUSIC);
    assert(settings_coordinator_is_dirty(coordinator));
    assert(settings_coordinator_current(coordinator)->selected_page ==
           APP_SETTINGS_PAGE_MUSIC);
    assert(!settings_coordinator_tick(coordinator, 0));
    assert(!settings_coordinator_tick(coordinator, 2999));
    assert(spy.calls == 0U);
    assert(settings_coordinator_tick(coordinator, 3000));
    assert(spy.calls == 1U);
    assert(!settings_coordinator_is_dirty(coordinator));
    assert(spy.saved.selected_page == APP_SETTINGS_PAGE_MUSIC);

    // Selecting the already-persisted page must not arm or write anything.
    settings_coordinator_page_changed(coordinator, APP_SETTINGS_PAGE_MUSIC);
    assert(!settings_coordinator_is_dirty(coordinator));
    assert(!settings_coordinator_tick(coordinator, 100000));
    assert(spy.calls == 1U);

    settings_coordinator_destroy(coordinator);
}

static void test_changes_reset_quiet_period_and_revert(void) {
    struct save_spy spy = {0};
    settings_coordinator_t *coordinator = new_coordinator(&spy);
    assert(coordinator != NULL);

    settings_coordinator_page_changed(coordinator, APP_SETTINGS_PAGE_MUSIC);
    assert(!settings_coordinator_tick(coordinator, 100));
    settings_coordinator_page_changed(coordinator,
                                      APP_SETTINGS_PAGE_NAVIGATION);
    assert(!settings_coordinator_tick(coordinator, 200));
    assert(!settings_coordinator_tick(coordinator, 3199));
    assert(settings_coordinator_tick(coordinator, 3200));
    assert(spy.calls == 1U);
    assert(spy.saved.selected_page == APP_SETTINGS_PAGE_NAVIGATION);

    // A change reverted before the deadline is equivalent to no change.
    settings_coordinator_page_changed(coordinator, APP_SETTINGS_PAGE_CLOCK);
    settings_coordinator_page_changed(coordinator,
                                      APP_SETTINGS_PAGE_NAVIGATION);
    assert(!settings_coordinator_is_dirty(coordinator));
    assert(!settings_coordinator_tick(coordinator, 10000));
    assert(spy.calls == 1U);

    settings_coordinator_destroy(coordinator);
}

static void test_save_failure_is_retried(void) {
    struct save_spy spy = {.fail = true};
    settings_coordinator_t *coordinator = new_coordinator(&spy);
    assert(coordinator != NULL);

    settings_coordinator_page_changed(coordinator, APP_SETTINGS_PAGE_FAULTS);
    assert(!settings_coordinator_tick(coordinator, 10));
    assert(!settings_coordinator_tick(coordinator, 3010));
    assert(spy.calls == 1U);
    assert(settings_coordinator_is_dirty(coordinator));

    spy.fail = false;
    assert(!settings_coordinator_tick(coordinator, 6009));
    assert(settings_coordinator_tick(coordinator, 6010));
    assert(spy.calls == 2U);
    assert(!settings_coordinator_is_dirty(coordinator));

    settings_coordinator_destroy(coordinator);
}

static void test_future_setting_setters(void) {
    struct save_spy spy = {0};
    settings_coordinator_t *coordinator = new_coordinator(&spy);
    assert(coordinator != NULL);

    assert(settings_coordinator_set_brightness(coordinator, 42));
    assert(settings_coordinator_set_units(coordinator,
                                          APP_SETTINGS_UNITS_IMPERIAL));
    assert(!settings_coordinator_set_brightness(coordinator, 101));
    assert(
        !settings_coordinator_set_theme(coordinator, APP_SETTINGS_THEME_COUNT));
    assert(!settings_coordinator_tick(coordinator, 0));
    assert(settings_coordinator_tick(coordinator, 3000));
    assert(spy.calls == 1U);
    assert(spy.saved.brightness_percent == 42U);
    assert(spy.saved.units == APP_SETTINGS_UNITS_IMPERIAL);

    settings_coordinator_destroy(coordinator);
}

static void test_startup_page(void) {
    struct save_spy spy = {0};
    settings_coordinator_t *coordinator = new_coordinator(&spy);
    assert(coordinator != NULL);

    assert(settings_coordinator_set_startup_page(coordinator,
                                                 APP_SETTINGS_PAGE_CLOCK));
    assert(settings_coordinator_is_dirty(coordinator));
    assert(!settings_coordinator_tick(coordinator, 0));
    assert(settings_coordinator_tick(coordinator, 3000));
    assert(spy.saved.startup_page == APP_SETTINGS_PAGE_CLOCK);

    assert(settings_coordinator_set_startup_page(
        coordinator, APP_SETTINGS_STARTUP_LAST_PAGE));
    assert(!settings_coordinator_set_startup_page(
        coordinator, APP_SETTINGS_STARTUP_LAST_PAGE + 1));
    assert(settings_coordinator_current(coordinator)->startup_page ==
           APP_SETTINGS_STARTUP_LAST_PAGE);

    settings_coordinator_destroy(coordinator);
}

int main(void) {
    test_selection_debounce_and_noop();
    test_changes_reset_quiet_period_and_revert();
    test_save_failure_is_retried();
    test_future_setting_setters();
    test_startup_page();
    puts("settings coordinator tests: OK");
    return 0;
}
