#include "domain/display_brightness.h"

#include <assert.h>

int main(void) {
    assert(display_brightness_level(0) == 0);
    assert(display_brightness_level(1) == 1);
    assert(display_brightness_level(50) == 8);
    assert(display_brightness_level(100) == 16);
    assert(display_brightness_level(255) == 16);
    return 0;
}
