#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Shared RGB888 palette for vector UI and RGB565 music covers.
// Pure constants: no LVGL or platform dependency.
#define AMBER_PALETTE_BG        0x170F08U
#define AMBER_PALETTE_BRIGHT    0xFF8C00U
#define AMBER_PALETTE_DIM       0x6B451BU
#define AMBER_PALETTE_SEPARATOR 0xA85C00U

// Inverted palette: reverse the four existing tones without adding colors.
// Swap background/foreground and the two intermediate contrast roles.
#define AMBER_PALETTE_INVERTED_BG        AMBER_PALETTE_BRIGHT
#define AMBER_PALETTE_INVERTED_BRIGHT    AMBER_PALETTE_BG
#define AMBER_PALETTE_INVERTED_DIM       AMBER_PALETTE_SEPARATOR
#define AMBER_PALETTE_INVERTED_SEPARATOR AMBER_PALETTE_DIM

typedef enum {
    AMBER_PALETTE_ROLE_BG = 0,
    AMBER_PALETTE_ROLE_BRIGHT,
    AMBER_PALETTE_ROLE_DIM,
    AMBER_PALETTE_ROLE_SEPARATOR,
    AMBER_PALETTE_ROLE_COUNT,
} amber_palette_role_t;

// RGB888 of `role` in the normal (`inverted` false) or inverted palette.
static inline uint32_t amber_palette_rgb(bool inverted,
                                         amber_palette_role_t role) {
    static const uint32_t k_colors[2][AMBER_PALETTE_ROLE_COUNT] = {
        {AMBER_PALETTE_BG, AMBER_PALETTE_BRIGHT, AMBER_PALETTE_DIM,
         AMBER_PALETTE_SEPARATOR},
        {AMBER_PALETTE_INVERTED_BG, AMBER_PALETTE_INVERTED_BRIGHT,
         AMBER_PALETTE_INVERTED_DIM, AMBER_PALETTE_INVERTED_SEPARATOR},
    };
    if ((int)role < 0 || role >= AMBER_PALETTE_ROLE_COUNT) {
        role = AMBER_PALETTE_ROLE_BG;
    }
    return k_colors[inverted ? 1 : 0][role];
}

// Exact role-preserving remap: if `rgb` is one of the four colors of the
// source palette, writes the color of the same role in the destination
// palette and returns true. Any other color is left alone (false). Because
// both palettes hold four distinct colors, normal -> inverted -> normal is
// the identity: toggling never drifts.
static inline bool amber_palette_remap_rgb(uint32_t rgb, bool from_inverted,
                                           bool to_inverted, uint32_t *out) {
    rgb &= 0xFFFFFFU;
    for (int role = 0; role < AMBER_PALETTE_ROLE_COUNT; role++) {
        if (amber_palette_rgb(from_inverted, (amber_palette_role_t)role) ==
            rgb) {
            if (out != NULL) {
                *out =
                    amber_palette_rgb(to_inverted, (amber_palette_role_t)role);
            }
            return true;
        }
    }
    return false;
}
