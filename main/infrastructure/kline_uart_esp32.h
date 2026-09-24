#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/uart.h"

#include "domain/kline_transport.h"

// Transport K-line pour ESP32-S3 au-dessus du pilote UART ESP-IDF.
//
// ⚠️ AVERTISSEMENT MATÉRIEL OBLIGATOIRE : la LILYGO T-RGB n'embarque pas de
// transceiver K-line. La ligne ISO-9141 12 V ne doit JAMAIS être raccordée
// directement à l'ESP32. Utiliser un transceiver automobile externe (L9637D,
// MC33290 ou équivalent qualifié) avec protections contre les transitoires,
// exposant des lignes TX/RX logiques compatibles 3,3 V.
//
// Écho local : selon le montage, l'UART peut relire ses propres octets émis
// (fil unique bouclé). Mettre `local_echo = true` pour que le transport rejette
// cet écho local après chaque écriture ; son absence ou sa corruption fait
// échouer l'écriture plutôt que de laisser la session consommer un flux ambigu.
// La couche session ne voit alors que l'écho de commande renvoyé par l'ECU.

typedef struct {
    uart_port_t uart_num; // ex. UART_NUM_1
    int tx_gpio;          // GPIO relié au TX du transceiver
    int rx_gpio;          // GPIO relié au RX du transceiver
    int baud_rate;        // 0 => 9600 (défaut MEMS)
    bool local_echo;      // rejeter l'écho local des octets émis
} kline_uart_config_t;

typedef struct kline_uart_s kline_uart_t;

// Installe le pilote UART (9600 8N1) et prépare le transport. Renvoie NULL en
// cas d'échec.
kline_uart_t *kline_uart_create(const kline_uart_config_t *config);

// Libère le pilote UART et l'instance.
void kline_uart_destroy(kline_uart_t *k);

// Vue `kline_transport_t` de cette instance (à passer à la session MEMS).
kline_transport_t kline_uart_transport(kline_uart_t *k);
