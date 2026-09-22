#include <assert.h>
#include <stdio.h>

#include "ui/navigation/dashboard_timing.h"

static void test_regular_period(void) {
    assert(!dashboard_period_elapsed(100, 50, 60));
    assert(dashboard_period_elapsed(110, 50, 60));
    assert(!dashboard_period_elapsed(110, 50, 0));
}

static void test_tick_wrap(void) {
    const uint32_t previous = UINT32_MAX - 10U;
    assert(!dashboard_period_elapsed(4U, previous, 16U));
    assert(dashboard_period_elapsed(5U, previous, 16U));
}

int main(void) {
    test_regular_period();
    test_tick_wrap();
    puts("dashboard timing tests: OK");
    return 0;
}
