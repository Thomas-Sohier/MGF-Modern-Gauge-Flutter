#include "infrastructure/kline_uart_esp32.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "kline_uart";

#define KLINE_RX_BUF_SIZE 256
#define KLINE_DEFAULT_BAUD 9600
#define KLINE_SLOW_INIT_BIT_MS 200  // 5 bauds

struct kline_uart_s {
    uart_port_t uart_num;
    int tx_gpio;
    int rx_gpio;
    bool local_echo;
};

static int kline_write(void *ctx, const uint8_t *buf, size_t len) {
    kline_uart_t *k = ctx;
    int written = uart_write_bytes(k->uart_num, (const char *)buf, len);
    if (written < 0) return -1;
    // Attendre l'émission complète pour respecter le rythme half-duplex.
    uart_wait_tx_done(k->uart_num, pdMS_TO_TICKS(50));

    // Rejeter l'écho local (fil unique bouclé) : lire et jeter le même nombre
    // d'octets que ceux émis.
    if (k->local_echo && written > 0) {
        uint8_t sink[16];
        int remaining = written;
        while (remaining > 0) {
            int chunk = remaining > (int)sizeof(sink) ? (int)sizeof(sink) : remaining;
            int n = uart_read_bytes(k->uart_num, sink, chunk, pdMS_TO_TICKS(50));
            if (n <= 0) break;  // écho absent : montage sans loopback local
            remaining -= n;
        }
    }
    return written;
}

static int kline_read(void *ctx, uint8_t *buf, size_t len, uint32_t timeout_ms) {
    kline_uart_t *k = ctx;
    int n = uart_read_bytes(k->uart_num, buf, len, pdMS_TO_TICKS(timeout_ms));
    return n;  // >=0 nombre d'octets, <0 erreur
}

static void kline_flush(void *ctx) {
    kline_uart_t *k = ctx;
    uart_flush_input(k->uart_num);
}

// Attend jusqu'à l'instant absolu `start_us + plus_ms` (limite la dérive du
// bit-bang, comme le sleepUntil du projet Go).
static void sleep_until(int64_t start_us, int plus_ms) {
    int64_t remaining_ms =
        (start_us + (int64_t)plus_ms * 1000 - esp_timer_get_time()) / 1000;
    if (remaining_ms > 0) vTaskDelay(pdMS_TO_TICKS(remaining_ms));
}

// Réveil « slow init » 5 bauds requis par MEMS 1.9 : émet l'adresse ECU bit à
// bit (LSB d'abord) en pilotant le GPIO TX à la main, puis rend le brochage au
// pilote UART. Portage de `MEMS19Reader.wakeUp` (ecureader_mems19.go).
// Niveaux : ligne au repos = haut (1) ; start bit = bas (0) ; stop bit = haut.
static int kline_wake_up(void *ctx, uint8_t ecu_address) {
    kline_uart_t *k = ctx;

    // Détacher le TX de l'UART pour le piloter en GPIO.
    gpio_reset_pin(k->tx_gpio);
    gpio_set_direction(k->tx_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(k->tx_gpio, 1);      // repos haut
    vTaskDelay(pdMS_TO_TICKS(2000));    // ligne stable avant le start bit

    const int b = KLINE_SLOW_INIT_BIT_MS;
    int64_t start = esp_timer_get_time();

    gpio_set_level(k->tx_gpio, 0);      // start bit
    sleep_until(start, b);
    for (int i = 0; i < 8; i++) {       // 8 bits de données, LSB d'abord
        gpio_set_level(k->tx_gpio, (ecu_address >> i) & 1);
        sleep_until(start, b + (i + 1) * b);
    }
    gpio_set_level(k->tx_gpio, 1);      // stop bit
    sleep_until(start, b + 9 * b);

    // Ré-attacher le TX à l'UART, purger les octets de synchro émis par l'ECU.
    esp_err_t err = uart_set_pin(k->uart_num, k->tx_gpio, k->rx_gpio,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "reattache UART après slow init: %s", esp_err_to_name(err));
        return -1;
    }
    uart_flush_input(k->uart_num);
    ESP_LOGI(TAG, "slow init 5 bauds émis (adresse ECU 0x%02X)", ecu_address);
    return 0;
}

kline_uart_t *kline_uart_create(const kline_uart_config_t *config) {
    if (config == NULL) return NULL;

    kline_uart_t *k = calloc(1, sizeof(*k));
    if (k == NULL) return NULL;
    k->uart_num = config->uart_num;
    k->tx_gpio = config->tx_gpio;
    k->rx_gpio = config->rx_gpio;
    k->local_echo = config->local_echo;

    const uart_config_t uart_config = {
        .baud_rate = config->baud_rate > 0 ? config->baud_rate : KLINE_DEFAULT_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(k->uart_num, KLINE_RX_BUF_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install: %s", esp_err_to_name(err));
        free(k);
        return NULL;
    }
    if ((err = uart_param_config(k->uart_num, &uart_config)) != ESP_OK ||
        (err = uart_set_pin(k->uart_num, config->tx_gpio, config->rx_gpio,
                            UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) != ESP_OK) {
        ESP_LOGE(TAG, "uart config/pin: %s", esp_err_to_name(err));
        uart_driver_delete(k->uart_num);
        free(k);
        return NULL;
    }

    ESP_LOGI(TAG, "K-line UART%d prête (tx=%d rx=%d, %d bps, echo local %s)",
             (int)k->uart_num, config->tx_gpio, config->rx_gpio,
             uart_config.baud_rate, k->local_echo ? "on" : "off");
    return k;
}

void kline_uart_destroy(kline_uart_t *k) {
    if (k == NULL) return;
    uart_driver_delete(k->uart_num);
    free(k);
}

kline_transport_t kline_uart_transport(kline_uart_t *k) {
    return (kline_transport_t){
        .write = kline_write,
        .read = kline_read,
        .flush = kline_flush,
        .wake_up = kline_wake_up,
        .ctx = k,
    };
}
