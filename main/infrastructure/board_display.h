#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"
#include "infrastructure/shared_i2c.h"

// LILYGO T-RGB 2.1 Full Circle H597: ST7701S RGB565, CST820 and the
// XL9535-compatible I2C expander.  Define MGF_LILYGO_T_RGB_V2=1 at build time
// to select the vendor V2 profile (timings, RGB GPIO order and init table).
lv_display_t *board_display_start(void);

// Stops the display and releases all board-owned handles. Call only after
// destroying borrowed RTC/UI objects; safe to call after a partial start.
void board_display_stop(void);

// AW9364 brightness is discrete: 0 is off, 1..16 are the hardware levels.
// The existing on/off API remains available for application callers.
void board_display_backlight_set_brightness(uint8_t level);

// Vue non propriétaire du bus I2C GPIO8/48 initialisé par l'affichage.
bool board_display_i2c_bus(shared_i2c_bus_t *out);
void board_display_backlight_on(void);
void board_display_backlight_off(void);

// `timeout_ms` = 0 waits indefinitely.  LVGL callbacks already own this lock.
bool board_display_lock(uint32_t timeout_ms);
void board_display_unlock(void);
