#include "domain/display_brightness.h"

uint8_t display_brightness_level(uint8_t percent) {
    if (percent == 0U) return 0U;
    if (percent > 100U) percent = 100U;
    return (uint8_t)(((percent - 1U) * 15U) / 99U + 1U);
}

uint8_t display_brightness_percent_for_level(uint8_t level) {
    if (level == 0U) return 0U;
    if (level > DISPLAY_BRIGHTNESS_LEVELS) level = DISPLAY_BRIGHTNESS_LEVELS;
    // Inverse de display_brightness_level : ceil((level - 1) * 99 / 15) + 1.
    return (uint8_t)(((level - 1U) * 99U + 14U) / 15U + 1U);
}
