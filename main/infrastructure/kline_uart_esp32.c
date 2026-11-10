#include "infrastructure/kline_uart_esp32.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "infrastructure/kline_board_config.h"

static const char *TAG = "kline_uart";

#define KLINE_RX_BUF_SIZE               256
#define KLINE_DEFAULT_BAUD              9600
#define KLINE_SLOW_INIT_BIT_MS          200 // 5 bauds
#define KLINE_SLOW_INIT_SYNC_TIMEOUT_MS 1000

static bool kline_uart_config_valid(const kline_uart_config_t *config) {
    if (config == NULL || config->uart_num < UART_NUM_0 ||
        config->uart_num > UART_NUM_2 ||
        !MGF_KLINE_PINS_VALID(config->tx_gpio, config->rx_gpio)) {
        return false;
    }
    // MEMS uses 9600 8N1. Zero means the documented default; accepting a
    // different baud here would make a seemingly valid build fail in-session.
    return config->baud_rate == 0 || config->baud_rate == KLINE_DEFAULT_BAUD;
}

struct kline_uart_s {
    uart_port_t uart_num;
    int tx_gpio;
    int rx_gpio;
    bool local_echo;
};

static int kline_write(void *ctx, const uint8_t *buf, size_t len) {
    kline_uart_t *k = ctx;
    if (k == NULL || (buf == NULL && len != 0) || len > INT32_MAX) return -1;
    if (len == 0) return 0;

    int written = uart_write_bytes(k->uart_num, (const char *)buf, len);
    if (written < 0 || (size_t)written != len) return -1;
    // Attendre l'émission complète pour respecter le rythme half-duplex.
    if (uart_wait_tx_done(k->uart_num, pdMS_TO_TICKS(50)) != ESP_OK) return -1;

    // Dans un montage avec loopback local, l'écho arrive avant la réponse ECU.
    // Une absence d'écho est une erreur lorsque l'option est activée : il ne
    // faut surtout pas laisser passer un flux partiellement consommé comme
    // s'il s'agissait d'une réponse valide.
    if (k->local_echo) {
        uint8_t echo[16];
        size_t offset = 0;
        while (offset < len) {
            size_t chunk = len - offset;
            if (chunk > sizeof(echo)) chunk = sizeof(echo);
            int n =
                uart_read_bytes(k->uart_num, echo, chunk, pdMS_TO_TICKS(50));
            if (n <= 0 || (size_t)n > chunk ||
                memcmp(echo, buf + offset, (size_t)n) != 0) {
                return -1;
            }
            offset += (size_t)n;
        }
    }
    return written;
}

static int kline_read(void *ctx, uint8_t *buf, size_t len,
                      uint32_t timeout_ms) {
    kline_uart_t *k = ctx;
    if (k == NULL || (buf == NULL && len != 0)) return -1;
    if (len == 0) return 0;
    int n = uart_read_bytes(k->uart_num, buf, len, pdMS_TO_TICKS(timeout_ms));
    return n; // >=0 nombre d'octets, <0 erreur
}

static void kline_flush(void *ctx) {
    kline_uart_t *k = ctx;
    if (k != NULL) uart_flush_input(k->uart_num);
}

// Attend jusqu'à l'instant absolu `start_us + plus_ms` (limite la dérive du
// bit-bang, comme le sleepUntil du projet Go). Robuste quel que soit
// CONFIG_FREERTOS_HZ : on dort par ticks en s'arrêtant au moins un tick avant
// l'échéance (vTaskDelay(n) peut durer jusqu'à n ticks pleins), puis on finit
// en attente active à la microseconde.
static void sleep_until(int64_t start_us, int plus_ms) {
    const int64_t deadline_us = start_us + (int64_t)plus_ms * 1000;
    const int64_t tick_us = 1000000 / (int64_t)configTICK_RATE_HZ;
    int64_t remaining_us = deadline_us - esp_timer_get_time();
    if (remaining_us > 2 * tick_us) {
        vTaskDelay((TickType_t)((remaining_us - tick_us) / tick_us));
        remaining_us = deadline_us - esp_timer_get_time();
    }
    if (remaining_us > 0) esp_rom_delay_us((uint32_t)remaining_us);
}

static bool kline_wait_for_slow_init_sync(kline_uart_t *k,
                                          uint32_t timeout_ms) {
    static const uint8_t sync[] = {0x55, 0x76, 0x83};
    const int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    size_t matched = 0;

    // The UART RX pin may have sampled the 5-baud waveform while TX was
    // bit-banged. Search for the sync sequence instead of assuming those
    // framing-error bytes are absent from the RX FIFO.
    while (matched < sizeof(sync)) {
        int64_t remaining_us = deadline - esp_timer_get_time();
        if (remaining_us <= 0) return false;
        uint32_t remaining_ms = (uint32_t)((remaining_us + 999) / 1000);
        uint8_t byte = 0;
        int n =
            uart_read_bytes(k->uart_num, &byte, 1, pdMS_TO_TICKS(remaining_ms));
        if (n <= 0) return false;
        if (byte == sync[matched]) {
            matched++;
        } else {
            matched = byte == sync[0] ? 1 : 0;
        }
    }
    return true;
}

// Rend le GPIO TX au pilote UART (après le bit-bang, ou sur erreur).
static esp_err_t kline_reattach_uart(kline_uart_t *k) {
    return uart_set_pin(k->uart_num, k->tx_gpio, k->rx_gpio, UART_PIN_NO_CHANGE,
                        UART_PIN_NO_CHANGE);
}

// Échec en cours de bit-bang : remettre la ligne au repos et le TX à l'UART
// pour qu'une nouvelle tentative reparte d'un état propre.
static int kline_wake_up_abort(kline_uart_t *k) {
    (void)gpio_set_level(k->tx_gpio, 1);
    esp_err_t err = kline_reattach_uart(k);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "reattache UART après échec slow init: %s",
                 esp_err_to_name(err));
    }
    return -1;
}

// Réveil « slow init » 5 bauds requis par MEMS 1.9 : émet l'adresse ECU bit à
// bit (LSB d'abord) en pilotant le GPIO TX à la main, puis rend le brochage au
// pilote UART. Portage de `MEMS19Reader.wakeUp` (ecureader_mems19.go).
// Niveaux : ligne au repos = haut (1) ; start bit = bas (0) ; stop bit = haut.
static int kline_wake_up(void *ctx, uint8_t ecu_address) {
    kline_uart_t *k = ctx;
    if (k == NULL) return -1;

    // Rejeter les résidus avant le bit-bang. Les éventuels octets parasites
    // générés pendant les 5 bauds sont ignorés par le chercheur de synchro.
    uart_flush_input(k->uart_num);

    // Détacher le TX de l'UART pour le piloter en GPIO.
    if (gpio_reset_pin(k->tx_gpio) != ESP_OK ||
        gpio_set_direction(k->tx_gpio, GPIO_MODE_OUTPUT) != ESP_OK ||
        gpio_set_level(k->tx_gpio, 1) != ESP_OK) {
        return kline_wake_up_abort(k);
    }
    vTaskDelay(pdMS_TO_TICKS(2000)); // ligne stable avant le start bit

    const int b = KLINE_SLOW_INIT_BIT_MS;
    int64_t start = esp_timer_get_time();

    if (gpio_set_level(k->tx_gpio, 0) != ESP_OK) // start bit
        return kline_wake_up_abort(k);
    sleep_until(start, b);
    for (int i = 0; i < 8; i++) { // 8 bits de données, LSB d'abord
        if (gpio_set_level(k->tx_gpio, (ecu_address >> i) & 1) != ESP_OK)
            return kline_wake_up_abort(k);
        sleep_until(start, b + (i + 1) * b);
    }
    if (gpio_set_level(k->tx_gpio, 1) != ESP_OK) // stop bit
        return kline_wake_up_abort(k);
    sleep_until(start, b + 9 * b);

    // Ré-attacher le TX à l'UART puis vérifier la synchronisation ECU. Les
    // trois octets sont attendus par MEMS 1.9 ; un ECU absent ou un mauvais
    // timing doit faire échouer la connexion, pas laisser un faux handshake.
    esp_err_t err = kline_reattach_uart(k);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "reattache UART après slow init: %s",
                 esp_err_to_name(err));
        return -1;
    }

    if (!kline_wait_for_slow_init_sync(k, KLINE_SLOW_INIT_SYNC_TIMEOUT_MS)) {
        ESP_LOGW(TAG, "réponse slow init MEMS 1.9 absente ou invalide");
        uart_flush_input(k->uart_num);
        return -1;
    }

    ESP_LOGI(TAG, "slow init 5 bauds validé (adresse ECU 0x%02X)", ecu_address);
    return 0;
}

kline_uart_t *kline_uart_create(const kline_uart_config_t *config) {
    if (!kline_uart_config_valid(config)) {
        ESP_LOGE(TAG, "configuration K-line/UART invalide");
        return NULL;
    }

    kline_uart_t *k = calloc(1, sizeof(*k));
    if (k == NULL) return NULL;
    k->uart_num = config->uart_num;
    k->tx_gpio = config->tx_gpio;
    k->rx_gpio = config->rx_gpio;
    k->local_echo = config->local_echo;

    const uart_config_t uart_config = {
        .baud_rate =
            config->baud_rate > 0 ? config->baud_rate : KLINE_DEFAULT_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err =
        uart_driver_install(k->uart_num, KLINE_RX_BUF_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install: %s", esp_err_to_name(err));
        free(k);
        return NULL;
    }
    if ((err = uart_param_config(k->uart_num, &uart_config)) != ESP_OK ||
        (err = uart_set_pin(k->uart_num, config->tx_gpio, config->rx_gpio,
                            UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)) !=
            ESP_OK) {
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
    if (k == NULL) return (kline_transport_t){0};
    return (kline_transport_t){
        .write = kline_write,
        .read = kline_read,
        .flush = kline_flush,
        .wake_up = kline_wake_up,
        .ctx = k,
    };
}
