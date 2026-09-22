#pragma once

#include <stdint.h>

// Maps the persisted percentage to the AW9364's 16 discrete levels.
// Zero remains off; non-zero values always produce a usable level.
uint8_t display_brightness_level(uint8_t percent);
