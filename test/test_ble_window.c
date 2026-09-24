#include <assert.h>
#include <stdio.h>

#include "domain/ble_window.h"

static void test_expiry(void) {
    ble_window_t window = {0};
    assert(!ble_window_tick(&window, 0, false));
    assert(ble_window_remaining_s(&window, 0) == 0);

    ble_window_open(&window, 1000);
    assert(ble_window_remaining_s(&window, 1000) == 300);
    assert(ble_window_remaining_s(&window, 1001) == 300);
    assert(ble_window_remaining_s(&window, 2000) == 299);
    assert(!ble_window_tick(&window, 1000 + BLE_WINDOW_DURATION_MS - 1, false));
    assert(ble_window_tick(&window, 1000 + BLE_WINDOW_DURATION_MS, false));
    assert(!window.open);
    // L'expiration n'est signalée qu'une fois.
    assert(!ble_window_tick(&window, 1000 + BLE_WINDOW_DURATION_MS + 1, false));
}

static void test_connection_extends(void) {
    ble_window_t window = {0};
    ble_window_open(&window, 0);
    assert(!ble_window_tick(&window, BLE_WINDOW_DURATION_MS + 10, true));
    assert(window.open);
    assert(!ble_window_tick(&window, BLE_WINDOW_DURATION_MS + 20, false));
    assert(ble_window_tick(&window, 2U * BLE_WINDOW_DURATION_MS + 10, false));
}

static void test_close_and_wrap(void) {
    ble_window_t window = {0};
    ble_window_open(&window, UINT32_MAX - 500U);
    assert(!ble_window_tick(&window, 100, false));
    assert(ble_window_remaining_s(&window, 100) > 298);
    ble_window_close(&window);
    assert(!ble_window_tick(&window, UINT32_MAX, false));
    assert(ble_window_remaining_s(&window, 100) == 0);
}

int main(void) {
    test_expiry();
    test_connection_extends();
    test_close_and_wrap();
    puts("BLE window tests: OK");
    return 0;
}
