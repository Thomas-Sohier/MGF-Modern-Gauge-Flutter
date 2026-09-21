#pragma once

#include <stddef.h>
#include <stdint.h>

// Abstraction minimale d'une liaison K-line (octet-flux half-duplex).
//
// Volontairement découplée du matériel : la logique de session MEMS
// (`mems_session.[ch]`) est écrite contre cette interface, ce qui permet de la
// tester sur hôte avec un transport factice et de brancher l'UART ESP-IDF réel
// (`infrastructure/kline_uart_esp32.[ch]`) sans la modifier.
//
// Sur K-line, l'ECU ré-émet (« echo ») chaque octet écrit : les implémentations
// half-duplex doivent donc soit désactiver l'écho matériel, soit laisser
// l'écho remonter dans le flux lu (le protocole MEMS attend justement l'écho de
// commande en tête de réponse).
typedef struct {
    // Écrit `len` octets. Renvoie le nombre d'octets écrits, ou < 0 en erreur.
    int (*write)(void *ctx, const uint8_t *buf, size_t len);
    // Lit jusqu'à `len` octets, en bloquant au plus `timeout_ms`. Renvoie le
    // nombre d'octets lus (0 = timeout), ou < 0 en erreur.
    int (*read)(void *ctx, uint8_t *buf, size_t len, uint32_t timeout_ms);
    // Vide le tampon de réception (rejette les octets en attente). Optionnel.
    void (*flush)(void *ctx);
    // Réveil « slow init » 5 bauds : émet l'adresse ECU (`ecu_address`) bit à
    // bit sur la K-line, comme l'exige MEMS 1.9 avant le handshake standard.
    // Renvoie >= 0 si effectué, < 0 en erreur. Optionnel (NULL => non supporté ;
    // requis pour la variante MEMS 1.9).
    int (*wake_up)(void *ctx, uint8_t ecu_address);
    void *ctx;
} kline_transport_t;
