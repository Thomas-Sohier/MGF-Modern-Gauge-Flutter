#pragma once

#include "lvgl.h"

typedef struct boot_screen boot_screen_t;

boot_screen_t *boot_screen_create(lv_obj_t *parent);
void boot_screen_destroy(boot_screen_t *screen);
