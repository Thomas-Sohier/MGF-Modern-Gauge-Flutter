#include "infrastructure/shared_i2c.h"

#include <limits.h>

static int timeout_value(uint32_t timeout_ms) {
    if (timeout_ms == 0) return 1000;
    return timeout_ms > INT_MAX ? INT_MAX : (int)timeout_ms;
}

static esp_err_t ensure_device(shared_i2c_master_context_t *context,
                               uint8_t address) {
    if (context == NULL) return ESP_ERR_INVALID_ARG;
    if (!context->active || context->bus == NULL) return ESP_ERR_INVALID_STATE;
    if (context->device != NULL) {
        return context->address == address ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = context->clock_hz,
    };
    const esp_err_t error =
        i2c_master_bus_add_device(context->bus, &config, &context->device);
    if (error == ESP_OK) context->address = address;
    return error;
}

static esp_err_t master_write(void *opaque, uint8_t address,
                              const uint8_t *data, size_t data_size,
                              uint32_t timeout_ms) {
    if (data == NULL || data_size == 0) return ESP_ERR_INVALID_ARG;
    shared_i2c_master_context_t *context = opaque;
    esp_err_t error = ensure_device(context, address);
    if (error != ESP_OK) return error;
    return i2c_master_transmit(context->device, data, data_size,
                               timeout_value(timeout_ms));
}

static esp_err_t master_write_read(void *opaque, uint8_t address,
                                   const uint8_t *write_data, size_t write_size,
                                   uint8_t *read_data, size_t read_size,
                                   uint32_t timeout_ms) {
    if (write_data == NULL || write_size == 0 || read_data == NULL ||
        read_size == 0)
        return ESP_ERR_INVALID_ARG;
    shared_i2c_master_context_t *context = opaque;
    esp_err_t error = ensure_device(context, address);
    if (error != ESP_OK) return error;
    return i2c_master_transmit_receive(context->device, write_data, write_size,
                                       read_data, read_size,
                                       timeout_value(timeout_ms));
}

void shared_i2c_master_bus_init(shared_i2c_bus_t *bus,
                                shared_i2c_master_context_t *context,
                                i2c_master_bus_handle_t master_bus,
                                uint32_t clock_hz, int sda_gpio, int scl_gpio) {
    if (bus == NULL || context == NULL) return;
    *context = (shared_i2c_master_context_t){
        .bus = master_bus,
        .clock_hz = clock_hz,
        .active = master_bus != NULL,
    };
    *bus = (shared_i2c_bus_t){
        .context = context,
        .write = master_write,
        .write_read = master_write_read,
        .sda_gpio = sda_gpio,
        .scl_gpio = scl_gpio,
    };
}

void shared_i2c_master_bus_deinit(shared_i2c_bus_t *bus,
                                  shared_i2c_master_context_t *context) {
    if (context == NULL) return;

    context->active = false;
    if (context->device != NULL && context->bus != NULL) {
        (void)i2c_master_bus_rm_device(context->device);
    }
    context->device = NULL;
    context->bus = NULL;
    context->address = 0;
    if (bus != NULL) *bus = (shared_i2c_bus_t){0};
}
