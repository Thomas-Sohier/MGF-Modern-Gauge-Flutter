#pragma once

#include <stdbool.h>
#include <stdint.h>

// Unsigned subtraction is defined modulo 2^32, which makes this comparison
// safe across the LVGL tick counter wrap as long as periods stay below 2^31 ms.
static inline bool dashboard_period_elapsed(uint32_t now, uint32_t previous,
                                            uint32_t period_ms) {
    return period_ms != 0 && (uint32_t)(now - previous) >= period_ms;
}
