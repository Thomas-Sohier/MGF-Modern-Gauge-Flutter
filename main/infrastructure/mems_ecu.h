#pragma once

#include <stdbool.h>

#include "domain/ecu_source.h"
#include "infrastructure/kline_uart_esp32.h"

// Variante d'ECU. MEMS 1.6 et 1.9 partagent le même protocole ; la 1.9 exige un
// réveil 5 bauds préalable (géré par le décorateur `mems19_reader`).
typedef enum {
    MEMS_VARIANT_1_6 = 0,  // handshake direct
    MEMS_VARIANT_1_9 = 1,  // réveil 5 bauds puis handshake
} mems_variant_t;

// Source ECU réelle : dialogue MEMS sur K-line depuis une tâche FreeRTOS
// dédiée. La tâche gère la (re)connexion et interroge périodiquement l'ECU ; le
// dernier instantané est publié derrière un mutex. La lecture (`ecu_source`)
// est non bloquante et copie cet instantané — même contrat que `fake_ecu`, donc
// interchangeable dans `app_main`/`dashboard_controller`.

typedef struct mems_ecu_s mems_ecu_t;

typedef struct {
    kline_uart_config_t kline;  // brochage/UART du transceiver K-line
    mems_variant_t variant;     // MEMS 1.6 (défaut) ou 1.9 (réveil 5 bauds)
    uint32_t poll_period_ms;    // 0 => 200 ms
    uint32_t reconnect_delay_ms;// 0 => 1000 ms
} mems_ecu_config_t;

// Crée l'instance (UART + session), sans démarrer la tâche.
mems_ecu_t *mems_ecu_create(const mems_ecu_config_t *config);

// Démarre la tâche de fond (connexion + polling). Idempotent.
bool mems_ecu_start(mems_ecu_t *ecu);

// Arrête la tâche et détruit l'instance.
void mems_ecu_destroy(mems_ecu_t *ecu);

// Copie l'instantané courant (thread-safe). `connected` reflète l'état réel.
bool mems_ecu_read(mems_ecu_t *ecu, ecu_data_t *out);

// Adaptateur vers l'abstraction commune des sources ECU.
ecu_source_t mems_ecu_source(mems_ecu_t *ecu);
