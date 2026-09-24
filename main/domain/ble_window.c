#include "domain/ble_window.h"

#include <stddef.h>

void ble_window_open(ble_window_t *window, uint32_t now_ms) {
    if (window == NULL) return;
    window->open = true;
    window->deadline_ms = now_ms + BLE_WINDOW_DURATION_MS;
}

void ble_window_close(ble_window_t *window) {
    if (window == NULL) return;
    window->open = false;
}

bool ble_window_tick(ble_window_t *window, uint32_t now_ms, bool connected) {
    if (window == NULL || !window->open) return false;
    if (connected) {
        window->deadline_ms = now_ms + BLE_WINDOW_DURATION_MS;
        return false;
    }
    // Comparaison signée : robuste au rebouclage du compteur de ticks.
    if ((int32_t)(now_ms - window->deadline_ms) < 0) return false;
    window->open = false;
    return true;
}

uint32_t ble_window_remaining_s(const ble_window_t *window, uint32_t now_ms) {
    if (window == NULL || !window->open) return 0;
    const int32_t remaining = (int32_t)(window->deadline_ms - now_ms);
    if (remaining <= 0) return 0;
    return ((uint32_t)remaining + 999U) / 1000U;
}
