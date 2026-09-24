#pragma once

#include <stdint.h>

// Maps the persisted percentage to the AW9364's 16 discrete levels.
// Zero remains off; non-zero values always produce a usable level.
uint8_t display_brightness_level(uint8_t percent);

#define DISPLAY_BRIGHTNESS_LEVELS 16U

// Plus petit pourcentage donnant `level` (0..16) : permet de régler la
// luminosité cran par cran sans pas « morts » entre deux niveaux matériels.
uint8_t display_brightness_percent_for_level(uint8_t level);
