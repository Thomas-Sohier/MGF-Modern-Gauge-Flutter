#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "ui/navigation/dashboard_hold.h"

static void test_timing_boundary(void) {
    dashboard_hold_t hold;
    dashboard_hold_begin(&hold, 5000U, 100, 100);
    assert(!dashboard_hold_should_fire(&hold, 5999U, true)); // 999 ms
    assert(dashboard_hold_should_fire(&hold, 6000U, true));  // 1000 ms
}

static void test_once_only(void) {
    dashboard_hold_t hold;
    dashboard_hold_begin(&hold, 0U, 100, 100);
    assert(!dashboard_hold_should_fire(&hold, 999U, true));
    assert(dashboard_hold_should_fire(&hold, 1000U, true));
    assert(!dashboard_hold_should_fire(&hold, 1000U, true));
    assert(!dashboard_hold_should_fire(&hold, 5000U, true));
}

static void test_outside_disc_does_not_fire(void) {
    dashboard_hold_t hold;
    dashboard_hold_begin(&hold, 0U, 100, 100);
    assert(!dashboard_hold_should_fire(&hold, 2000U, false));
    assert(dashboard_hold_should_fire(&hold, 2001U, true));
}

static void test_tick_wrap(void) {
    dashboard_hold_t hold;
    dashboard_hold_begin(&hold, UINT32_MAX - 499U, 100, 100);
    assert(!dashboard_hold_should_fire(&hold, 499U, true)); // 999 ms
    dashboard_hold_begin(&hold, UINT32_MAX - 499U, 100, 100);
    assert(dashboard_hold_should_fire(&hold, 500U, true)); // 1000 ms
}

static void test_move_threshold(void) {
    dashboard_hold_t hold;

    // 17 px : autorisé.
    dashboard_hold_begin(&hold, 0U, 200, 200);
    assert(!dashboard_hold_move_cancelled(&hold, 217, 200, 480));
    assert(dashboard_hold_should_fire(&hold, 1000U, true));

    // 18 px pile : autorisé, c'est la limite.
    dashboard_hold_begin(&hold, 0U, 200, 200);
    assert(!dashboard_hold_move_cancelled(&hold, 218, 200, 480));
    assert(dashboard_hold_should_fire(&hold, 1000U, true));

    // 19 px : annulé.
    dashboard_hold_begin(&hold, 0U, 200, 200);
    assert(dashboard_hold_move_cancelled(&hold, 219, 200, 480));
    assert(!dashboard_hold_should_fire(&hold, 5000U, true));
}

static void test_strictly_greater_on_diagonal(void) {
    dashboard_hold_t hold;
    dashboard_hold_begin(&hold, 0U, 200, 200);
    // Distance euclidienne (13, 13) ≈ 18,38 px > 18 px.
    assert(dashboard_hold_move_cancelled(&hold, 213, 213, 480));
    assert(!dashboard_hold_should_fire(&hold, 5000U, true));
}

static void test_back_to_origin_stays_cancelled(void) {
    dashboard_hold_t hold;
    dashboard_hold_begin(&hold, 0U, 200, 200);
    assert(dashboard_hold_move_cancelled(&hold, 219, 200, 480));
    // Retour au point de départ : l'annulation tient pour toute la pression.
    assert(dashboard_hold_move_cancelled(&hold, 200, 200, 480));
    assert(hold.cancelled);
    assert(!dashboard_hold_should_fire(&hold, 5000U, true));
}

static void test_new_press_rearms(void) {
    dashboard_hold_t hold;
    dashboard_hold_begin(&hold, 0U, 200, 200);
    assert(dashboard_hold_move_cancelled(&hold, 300, 200, 480));
    assert(!dashboard_hold_should_fire(&hold, 1000U, true));

    // Nouvelle pression : l'état repart propre.
    dashboard_hold_begin(&hold, 2000U, 200, 200);
    assert(!hold.cancelled);
    assert(!hold.fired);
    assert(dashboard_hold_should_fire(&hold, 3000U, true));
}

static void test_gesture_cancels(void) {
    dashboard_hold_t hold;
    dashboard_hold_begin(&hold, 0U, 200, 200);
    dashboard_hold_cancel(&hold);
    assert(hold.cancelled);
    assert(!dashboard_hold_should_fire(&hold, 1000U, true));
    assert(!dashboard_hold_should_fire(&hold, 9000U, true));
}

static void test_untracked_state_never_fires(void) {
    dashboard_hold_t hold;
    dashboard_hold_reset(&hold);
    assert(!dashboard_hold_should_fire(&hold, 5000U, true));
    assert(!dashboard_hold_move_cancelled(&hold, 9999, 9999, 480));
}

int main(void) {
    test_timing_boundary();
    test_once_only();
    test_outside_disc_does_not_fire();
    test_tick_wrap();
    test_move_threshold();
    test_strictly_greater_on_diagonal();
    test_back_to_origin_stays_cancelled();
    test_new_press_rearms();
    test_gesture_cancels();
    test_untracked_state_never_fires();
    puts("dashboard hold tests: OK");
    return 0;
}
