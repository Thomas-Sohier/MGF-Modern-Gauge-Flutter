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

#if MGF_KLINE_TX_GPIO < 0 || MGF_KLINE_TX_GPIO > 48 ||                         \
    MGF_KLINE_RX_GPIO < 0 || MGF_KLINE_RX_GPIO > 48
#error "MGF_KLINE_TX_GPIO and MGF_KLINE_RX_GPIO must be ESP32-S3 GPIO numbers"
#endif

#if MGF_KLINE_TX_GPIO == MGF_KLINE_RX_GPIO
#error "K-line TX and RX must use different GPIOs"
#endif

// GPIO34..39 are input-only on the ESP32-S3; they can receive K-line data but
// cannot drive the transceiver's TX input. GPIO38 is therefore RX-only in the
// default LILYGO profile.
#if MGF_KLINE_TX_GPIO == 34 || MGF_KLINE_TX_GPIO == 35 ||                      \
    MGF_KLINE_TX_GPIO == 36 || MGF_KLINE_TX_GPIO == 37 ||                      \
    MGF_KLINE_TX_GPIO == 38
#error "K-line TX GPIO must be output-capable on ESP32-S3"
#endif

// LCD RGB, sync, backlight, and the LCD's serial data pins on the H597.
#if MGF_KLINE_TX_GPIO == 2 || MGF_KLINE_TX_GPIO == 3 ||                        \
    MGF_KLINE_TX_GPIO == 5 || MGF_KLINE_TX_GPIO == 6 ||                        \
    MGF_KLINE_TX_GPIO == 7 || MGF_KLINE_TX_GPIO == 9 ||                        \
    MGF_KLINE_TX_GPIO == 10 || MGF_KLINE_TX_GPIO == 11 ||                      \
    MGF_KLINE_TX_GPIO == 12 || MGF_KLINE_TX_GPIO == 13 ||                      \
    MGF_KLINE_TX_GPIO == 14 || MGF_KLINE_TX_GPIO == 15 ||                      \
    MGF_KLINE_TX_GPIO == 16 || MGF_KLINE_TX_GPIO == 17 ||                      \
    MGF_KLINE_TX_GPIO == 18 || MGF_KLINE_TX_GPIO == 21 ||                      \
    MGF_KLINE_TX_GPIO == 41 || MGF_KLINE_TX_GPIO == 42 ||                      \
    MGF_KLINE_TX_GPIO == 43 || MGF_KLINE_TX_GPIO == 44 ||                      \
    MGF_KLINE_TX_GPIO == 45 || MGF_KLINE_TX_GPIO == 46 ||                      \
    MGF_KLINE_TX_GPIO == 47 || MGF_KLINE_RX_GPIO == 2 ||                       \
    MGF_KLINE_RX_GPIO == 3 || MGF_KLINE_RX_GPIO == 5 ||                        \
    MGF_KLINE_RX_GPIO == 6 || MGF_KLINE_RX_GPIO == 7 ||                        \
    MGF_KLINE_RX_GPIO == 9 || MGF_KLINE_RX_GPIO == 10 ||                       \
    MGF_KLINE_RX_GPIO == 11 || MGF_KLINE_RX_GPIO == 12 ||                      \
    MGF_KLINE_RX_GPIO == 13 || MGF_KLINE_RX_GPIO == 14 ||                      \
    MGF_KLINE_RX_GPIO == 15 || MGF_KLINE_RX_GPIO == 16 ||                      \
    MGF_KLINE_RX_GPIO == 17 || MGF_KLINE_RX_GPIO == 18 ||                      \
    MGF_KLINE_RX_GPIO == 21 || MGF_KLINE_RX_GPIO == 41 ||                      \
    MGF_KLINE_RX_GPIO == 42 || MGF_KLINE_RX_GPIO == 43 ||                      \
    MGF_KLINE_RX_GPIO == 44 || MGF_KLINE_RX_GPIO == 45 ||                      \
    MGF_KLINE_RX_GPIO == 46 || MGF_KLINE_RX_GPIO == 47
#error "K-line GPIO conflicts with a LILYGO T-RGB LCD/display pin"
#endif

// Shared I2C/touch, power, USB, boot, battery, and the remaining SD signal.
// GPIO38 and GPIO40 are intentionally absent: they are the K-line defaults.
#if MGF_KLINE_TX_GPIO == 0 || MGF_KLINE_TX_GPIO == 1 ||                        \
    MGF_KLINE_TX_GPIO == 4 || MGF_KLINE_TX_GPIO == 8 ||                        \
    MGF_KLINE_TX_GPIO == 19 || MGF_KLINE_TX_GPIO == 20 ||                      \
    MGF_KLINE_TX_GPIO == 39 || MGF_KLINE_TX_GPIO == 48 ||                      \
    MGF_KLINE_RX_GPIO == 0 || MGF_KLINE_RX_GPIO == 1 ||                        \
    MGF_KLINE_RX_GPIO == 4 || MGF_KLINE_RX_GPIO == 8 ||                        \
    MGF_KLINE_RX_GPIO == 19 || MGF_KLINE_RX_GPIO == 20 ||                      \
    MGF_KLINE_RX_GPIO == 39 || MGF_KLINE_RX_GPIO == 48
#error "K-line GPIO conflicts with a reserved LILYGO T-RGB board resource"
#endif

#if MGF_LILYGO_SDMMC_ENABLED
#error "SDMMC must remain disabled when K-line owns GPIO38/GPIO40"
#endif
