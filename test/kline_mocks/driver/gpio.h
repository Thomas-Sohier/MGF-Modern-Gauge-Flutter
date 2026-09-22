#pragma once

#include "esp_err.h"

typedef enum { GPIO_MODE_OUTPUT = 1 } gpio_mode_t;

esp_err_t gpio_reset_pin(int gpio_num);
esp_err_t gpio_set_direction(int gpio_num, gpio_mode_t mode);
esp_err_t gpio_set_level(int gpio_num, unsigned int level);
