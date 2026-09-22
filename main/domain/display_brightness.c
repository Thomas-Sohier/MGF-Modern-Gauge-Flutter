#include "domain/display_brightness.h"

uint8_t display_brightness_level(uint8_t percent) {
    if (percent == 0U) return 0U;
    if (percent > 100U) percent = 100U;
    return (uint8_t)(((percent - 1U) * 15U) / 99U + 1U);
}
