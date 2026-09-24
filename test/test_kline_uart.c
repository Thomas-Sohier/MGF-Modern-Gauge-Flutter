#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "driver/gpio.h"
#include "infrastructure/kline_uart_esp32.h"

static struct {
    uint8_t rx[128];
    size_t len;
    size_t pos;
    bool loopback;
    bool partial_reads;
    bool sync_on_reattach;
    uint8_t gpio_levels[16];
    int64_t gpio_times[16];
    uint8_t last_read[16];
    size_t last_read_count;
    size_t gpio_level_count;
    esp_err_t wait_result;
    int64_t now_us;
} uart_mock;

static void enqueue(const uint8_t *data, size_t len) {
    assert(len <= sizeof(uart_mock.rx) - uart_mock.len);
    memcpy(uart_mock.rx + uart_mock.len, data, len);
    uart_mock.len += len;
}

const char *esp_err_to_name(esp_err_t error) {
    (void)error;
    return "mock";
}

esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size,
                              int tx_buffer_size, int queue_size, void *queue,
                              int intr_alloc_flags) {
    (void)uart_num;
    (void)rx_buffer_size;
    (void)tx_buffer_size;
    (void)queue_size;
    (void)queue;
    (void)intr_alloc_flags;
    return ESP_OK;
}

esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t *config) {
    (void)uart_num;
    (void)config;
    return ESP_OK;
}

esp_err_t uart_set_pin(uart_port_t uart_num, int tx, int rx, int rts, int cts) {
    (void)uart_num;
    (void)rx;
    (void)rts;
    (void)cts;
    if (uart_mock.sync_on_reattach && tx == 40) {
        const uint8_t sync[] = {0x00, 0x55, 0x76, 0x83};
        enqueue(sync, sizeof(sync));
        uart_mock.sync_on_reattach = false;
    }
    return ESP_OK;
}

int uart_write_bytes(uart_port_t uart_num, const char *src, size_t size) {
    (void)uart_num;
    if (uart_mock.loopback) enqueue((const uint8_t *)src, size);
    return (int)size;
}

esp_err_t uart_wait_tx_done(uart_port_t uart_num, TickType_t ticks) {
    (void)uart_num;
    (void)ticks;
    return uart_mock.wait_result;
}

int uart_read_bytes(uart_port_t uart_num, void *buffer, uint32_t length,
                    TickType_t ticks) {
    (void)uart_num;
    size_t available = uart_mock.len - uart_mock.pos;
    if (available == 0) {
        uart_mock.now_us += (int64_t)ticks * 1000;
        return 0;
    }
    size_t count = available < length ? available : length;
    if (uart_mock.partial_reads && count > 1) count = 1;
    memcpy(buffer, uart_mock.rx + uart_mock.pos, count);
    memcpy(uart_mock.last_read + uart_mock.last_read_count, buffer, count);
    uart_mock.last_read_count += count;
    uart_mock.pos += count;
    if (uart_mock.pos == uart_mock.len) uart_mock.pos = uart_mock.len = 0;
    return (int)count;
}

esp_err_t uart_flush_input(uart_port_t uart_num) {
    (void)uart_num;
    uart_mock.pos = uart_mock.len = 0;
    return ESP_OK;
}

esp_err_t uart_driver_delete(uart_port_t uart_num) {
    (void)uart_num;
    return ESP_OK;
}

esp_err_t gpio_reset_pin(int gpio_num) {
    (void)gpio_num;
    return ESP_OK;
}

esp_err_t gpio_set_direction(int gpio_num, gpio_mode_t mode) {
    (void)gpio_num;
    (void)mode;
    return ESP_OK;
}

esp_err_t gpio_set_level(int gpio_num, unsigned int level) {
    (void)gpio_num;
    assert(uart_mock.gpio_level_count < sizeof(uart_mock.gpio_levels));
    uart_mock.gpio_levels[uart_mock.gpio_level_count] = (uint8_t)level;
    uart_mock.gpio_times[uart_mock.gpio_level_count] = uart_mock.now_us;
    uart_mock.gpio_level_count++;
    return ESP_OK;
}

int64_t esp_timer_get_time(void) {
    return uart_mock.now_us;
}

void vTaskDelay(TickType_t ticks) {
    uart_mock.now_us += (int64_t)ticks * 1000;
}

static kline_uart_config_t valid_config(bool local_echo) {
    return (kline_uart_config_t){
        .uart_num = UART_NUM_1,
        .tx_gpio = 40,
        .rx_gpio = 38,
        .baud_rate = 0,
        .local_echo = local_echo,
    };
}

static void test_runtime_configuration_validation(void) {
    kline_uart_config_t config = valid_config(false);
    kline_uart_t *k = kline_uart_create(&config);
    assert(k != NULL);
    kline_uart_destroy(k);

    config.uart_num = 3;
    assert(kline_uart_create(&config) == NULL);
    config = valid_config(false);
    config.tx_gpio = config.rx_gpio;
    assert(kline_uart_create(&config) == NULL);
    config = valid_config(false);
    config.tx_gpio = 48;
    assert(kline_uart_create(&config) == NULL);
    config = valid_config(false);
    config.rx_gpio = 39;
    assert(kline_uart_create(&config) == NULL);
    config = valid_config(false);
    config.baud_rate = 10400;
    assert(kline_uart_create(&config) == NULL);
    config = valid_config(false);
    config.tx_gpio = 38;
    assert(kline_uart_create(&config) == NULL);
}

static void test_transport_and_local_echo(void) {
    memset(&uart_mock, 0, sizeof(uart_mock));
    uart_mock.wait_result = ESP_OK;
    kline_uart_config_t config = valid_config(true);
    kline_uart_t *k = kline_uart_create(&config);
    assert(k != NULL);
    kline_transport_t transport = kline_uart_transport(k);

    const uint8_t command[] = {0xCA, 0x75};
    uart_mock.loopback = true;
    assert(transport.write(transport.ctx, command, sizeof(command)) == 2);
    assert(uart_mock.len == 0);

    const uint8_t response[] = {0xCA, 0x75, 0xF4};
    enqueue(response, sizeof(response));
    uint8_t received[sizeof(response)] = {0};
    assert(transport.read(transport.ctx, received, sizeof(received), 10) ==
           (int)sizeof(response));
    assert(memcmp(received, response, sizeof(response)) == 0);

    uart_mock.loopback = false;
    assert(transport.write(transport.ctx, command, sizeof(command)) < 0);
    kline_uart_destroy(k);
}

static void test_slow_init_validates_sync(void) {
    memset(&uart_mock, 0, sizeof(uart_mock));
    uart_mock.wait_result = ESP_OK;
    kline_uart_config_t config = valid_config(false);
    kline_uart_t *k = kline_uart_create(&config);
    assert(k != NULL);
    uart_mock.sync_on_reattach = true;
    kline_transport_t transport = kline_uart_transport(k);
    assert(transport.wake_up(transport.ctx, 0x16) == 0);
    const uint8_t expected[] = {0, 0, 1, 1, 0, 1, 0, 0, 0, 1};
    assert(uart_mock.gpio_level_count >= sizeof(expected));
    assert(memcmp(uart_mock.gpio_levels + 1, expected, sizeof(expected)) == 0);
    for (size_t i = 2; i <= sizeof(expected); i++)
        assert(uart_mock.gpio_times[i] - uart_mock.gpio_times[i - 1] == 200000);
    kline_uart_destroy(k);

    memset(&uart_mock, 0, sizeof(uart_mock));
    uart_mock.wait_result = ESP_OK;
    k = kline_uart_create(&config);
    assert(k != NULL);
    transport = kline_uart_transport(k);
    assert(transport.wake_up(transport.ctx, 0x16) < 0);
    kline_uart_destroy(k);
}

static void test_local_echo_reads_partial_chunks(void) {
    memset(&uart_mock, 0, sizeof(uart_mock));
    uart_mock.wait_result = ESP_OK;
    uart_mock.loopback = true;
    uart_mock.partial_reads = true;
    kline_uart_config_t config = valid_config(true);
    kline_uart_t *k = kline_uart_create(&config);
    assert(k != NULL);
    kline_transport_t transport = kline_uart_transport(k);
    const uint8_t command[] = {0xD0, 0x80, 0x7D};
    assert(transport.write(transport.ctx, command, sizeof(command)) == 3);
    kline_uart_destroy(k);
}

int main(void) {
    test_runtime_configuration_validation();
    test_transport_and_local_echo();
    test_slow_init_validates_sync();
    test_local_echo_reads_partial_chunks();
    puts("kline uart tests: OK");
    return 0;
}
