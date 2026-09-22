#pragma once

#include <stdint.h>

#include "domain/rtc.h"
#include "infrastructure/shared_i2c.h"

#define DS3231_I2C_ADDRESS 0x68u
#define DS3231_I2C_SDA_GPIO 8
#define DS3231_I2C_SCL_GPIO 48

typedef struct {
    // Le bus est fourni par le propriétaire (display/LILYGO), jamais installé
    // ni désinstallé par le RTC. Le propriétaire doit détruire la RTC avant
    // de détruire le bus et ses vues empruntées.
    shared_i2c_bus_t bus;
    uint8_t address; // 0 -> DS3231_I2C_ADDRESS
    uint32_t timeout_ms;
} ds3231_config_t;

// Ne fait aucune transaction : l'absence physique est signalée par
// rtc_probe(), et ne doit pas empêcher le démarrage de l'application.
rtc_t *ds3231_create(const ds3231_config_t *config);
