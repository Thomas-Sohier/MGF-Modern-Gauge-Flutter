#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

// Vue non propriétaire d'un bus I2C déjà initialisé par le bring-up carte.
typedef esp_err_t (*shared_i2c_write_fn)(
    void *context, uint8_t address, const uint8_t *data, size_t data_size,
    uint32_t timeout_ms);
typedef esp_err_t (*shared_i2c_write_read_fn)(
    void *context, uint8_t address, const uint8_t *write_data,
    size_t write_size, uint8_t *read_data, size_t read_size,
    uint32_t timeout_ms);

typedef struct {
    void *context;
    shared_i2c_write_fn write;
    shared_i2c_write_read_fn write_read;
    int sda_gpio;
    int scl_gpio;
} shared_i2c_bus_t;

// Adaptateur du nouveau pilote I2C master ESP-IDF. Le propriétaire du bus
// conserve sa durée de vie. Un contexte est dédié à un périphérique/adresse.
typedef struct {
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    uint8_t address;
    uint32_t clock_hz;
    bool active;
} shared_i2c_master_context_t;

void shared_i2c_master_bus_init(shared_i2c_bus_t *bus,
                                shared_i2c_master_context_t *context,
                                i2c_master_bus_handle_t master_bus,
                                uint32_t clock_hz, int sda_gpio, int scl_gpio);

// Removes the adapter-owned device and invalidates all borrowed bus views. The
// underlying master bus is owned and deleted by the board bring-up.
void shared_i2c_master_bus_deinit(shared_i2c_bus_t *bus,
                                  shared_i2c_master_context_t *context);
