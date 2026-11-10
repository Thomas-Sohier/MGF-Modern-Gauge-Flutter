#pragma once

// Build-time resource policy for the LILYGO T-RGB 2.1 (H597) K-line port.
//
// GPIO38 and GPIO40 are the former microSD DAT0/CMD lines.  They are reclaimed
// for the UART only because this project deliberately does not initialize or
// own the SDMMC peripheral.  Keep this header free of ESP-IDF includes so its
// defaults and safeguards can also be checked by the host test suite.

#ifndef MGF_USE_MEMS_KLINE
#define MGF_USE_MEMS_KLINE 0
#endif

#ifndef MGF_KLINE_UART_NUM
#define MGF_KLINE_UART_NUM 1
#endif

#ifndef MGF_KLINE_TX_GPIO
#define MGF_KLINE_TX_GPIO 40
#endif

#ifndef MGF_KLINE_RX_GPIO
#define MGF_KLINE_RX_GPIO 38
#endif

#ifndef MGF_KLINE_LOCAL_ECHO
#define MGF_KLINE_LOCAL_ECHO 0
#endif

#if MGF_USE_MEMS_KLINE != 0 && MGF_USE_MEMS_KLINE != 1
#error "MGF_USE_MEMS_KLINE must be 0 or 1"
#endif

#if MGF_KLINE_LOCAL_ECHO != 0 && MGF_KLINE_LOCAL_ECHO != 1
#error "MGF_KLINE_LOCAL_ECHO must be 0 or 1"
#endif

// There is intentionally no SDMMC owner in the LILYGO K-line profile.
#ifndef MGF_LILYGO_SDMMC_ENABLED
#define MGF_LILYGO_SDMMC_ENABLED 0
#endif

#if MGF_KLINE_UART_NUM < 0 || MGF_KLINE_UART_NUM > 2
#error "MGF_KLINE_UART_NUM must select an ESP32-S3 UART (0, 1, or 2)"
#endif

// --- Ressources GPIO : SOURCE UNIQUE de vérité ------------------------------
// Ces prédicats sont évaluables par le préprocesseur (#if ci-dessous) ET en C
// (validation à l'exécution dans `kline_uart_esp32.c`).

#define MGF_KLINE_GPIO_IS_VALID(g) ((g) >= 0 && (g) <= 48)

// GPIO34..38 are input-only for this purpose on the ESP32-S3 profile; they can
// receive K-line data but cannot drive the transceiver's TX input. GPIO38 is
// therefore RX-only in the default LILYGO profile (GPIO39 is reserved below).
#define MGF_KLINE_GPIO_IS_INPUT_ONLY(g) ((g) >= 34 && (g) <= 38)

// LCD RGB, sync, backlight, and the LCD's serial data pins on the H597.
#define MGF_KLINE_GPIO_IS_DISPLAY(g)                                           \
    ((g) == 2 || (g) == 3 || ((g) >= 5 && (g) <= 7) ||                         \
     ((g) >= 9 && (g) <= 18) || (g) == 21 || ((g) >= 41 && (g) <= 47))

// Shared I2C/touch, power, USB, boot, battery, and the remaining SD signal.
// GPIO38 and GPIO40 are intentionally absent: they are the K-line defaults.
#define MGF_KLINE_GPIO_IS_BOARD_RESERVED(g)                                    \
    ((g) == 0 || (g) == 1 || (g) == 4 || (g) == 8 || (g) == 19 || (g) == 20 || \
     (g) == 39 || (g) == 48)

#define MGF_KLINE_GPIO_IS_RESERVED(g)                                          \
    (MGF_KLINE_GPIO_IS_DISPLAY(g) || MGF_KLINE_GPIO_IS_BOARD_RESERVED(g))

// Paire TX/RX utilisable pour la K-line sur cette carte.
#define MGF_KLINE_PINS_VALID(tx, rx)                                           \
    (MGF_KLINE_GPIO_IS_VALID(tx) && MGF_KLINE_GPIO_IS_VALID(rx) &&             \
     (tx) != (rx) && !MGF_KLINE_GPIO_IS_INPUT_ONLY(tx) &&                      \
     !MGF_KLINE_GPIO_IS_RESERVED(tx) && !MGF_KLINE_GPIO_IS_RESERVED(rx))

#if !MGF_KLINE_GPIO_IS_VALID(MGF_KLINE_TX_GPIO) ||                             \
    !MGF_KLINE_GPIO_IS_VALID(MGF_KLINE_RX_GPIO)
#error "MGF_KLINE_TX_GPIO and MGF_KLINE_RX_GPIO must be ESP32-S3 GPIO numbers"
#endif

#if MGF_KLINE_TX_GPIO == MGF_KLINE_RX_GPIO
#error "K-line TX and RX must use different GPIOs"
#endif

#if MGF_KLINE_GPIO_IS_INPUT_ONLY(MGF_KLINE_TX_GPIO)
#error "K-line TX GPIO must be output-capable on ESP32-S3"
#endif

#if MGF_KLINE_GPIO_IS_DISPLAY(MGF_KLINE_TX_GPIO) ||                            \
    MGF_KLINE_GPIO_IS_DISPLAY(MGF_KLINE_RX_GPIO)
#error "K-line GPIO conflicts with a LILYGO T-RGB LCD/display pin"
#endif

#if MGF_KLINE_GPIO_IS_BOARD_RESERVED(MGF_KLINE_TX_GPIO) ||                     \
    MGF_KLINE_GPIO_IS_BOARD_RESERVED(MGF_KLINE_RX_GPIO)
#error "K-line GPIO conflicts with a reserved LILYGO T-RGB board resource"
#endif

#if MGF_LILYGO_SDMMC_ENABLED
#error "SDMMC must remain disabled when K-line owns GPIO38/GPIO40"
#endif
