#include "domain/display_brightness.h"

#include <assert.h>

int main(void) {
    assert(display_brightness_level(0) == 0);
    assert(display_brightness_level(1) == 1);
    assert(display_brightness_level(50) == 8);
    assert(display_brightness_level(100) == 16);
    assert(display_brightness_level(255) == 16);

    assert(display_brightness_percent_for_level(0) == 0);
    assert(display_brightness_percent_for_level(1) == 1);
    assert(display_brightness_percent_for_level(16) == 100);
    assert(display_brightness_percent_for_level(40) == 100);
    for (uint8_t level = 0; level <= DISPLAY_BRIGHTNESS_LEVELS; level++) {
        const uint8_t percent = display_brightness_percent_for_level(level);
        assert(display_brightness_level(percent) == level);
        // Plus petit pourcentage du niveau : un de moins retombe en dessous.
        if (percent > 0) assert(display_brightness_level(percent - 1) < level);
    }
    return 0;
}
