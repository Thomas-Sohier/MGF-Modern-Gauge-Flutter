#include "infrastructure/mems_ecu.h"

#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "domain/ecu_freshness.h"
#include "domain/mems19_reader.h"
#include "domain/mems_reader.h"
#include "domain/mems_session.h"

static const char *TAG = "mems_ecu";

#define DEFAULT_POLL_MS      100
#define DEFAULT_RECONNECT_MS 1000
#define READ_TIMEOUT_MS      100
#define TASK_STACK           4096
#define TASK_PRIO            5

struct mems_ecu_s {
    kline_uart_t *kline;
    mems_reader_t base_reader; // impl commune (adresses stables : dans le tas)
    mems19_reader_t reader19;  // décorateur 1.9 (utilisé si variant == 1.9)
    mems_session_t session;
    uint32_t poll_period_ms;
    uint32_t reconnect_delay_ms;

    SemaphoreHandle_t lock; // protège `snapshot`, `snapshot_us`, `has_snapshot`
    SemaphoreHandle_t stopped; // signal de fin de la tâche avant libération
    ecu_data_t snapshot;       // dernier instantané publié
    int64_t snapshot_us;       // horodatage monotone de la publication (µs)
    bool has_snapshot;         // au moins un instantané publié depuis le boot

    TaskHandle_t task;
    volatile bool running;

    // Notification hors verrou des transitions de connexion (cf. config).
    // Écrit uniquement par la tâche, d'où l'absence de synchronisation propre.
    void (*on_connection_changed)(void *ctx, bool connected);
    void *connection_ctx;
    bool last_notified_connected;
    bool has_notified_connection;
};

static void publish(mems_ecu_t *ecu, const ecu_data_t *data) {
    if (xSemaphoreTake(ecu->lock, portMAX_DELAY) == pdTRUE) {
        ecu->snapshot = *data;
        ecu->snapshot_us = esp_timer_get_time();
        ecu->has_snapshot = true;
        xSemaphoreGive(ecu->lock);
    }

    // Hors mutex : le callback remonte vers d'autres services (diagnostics),
    // qui ne doivent pas être appelés sous le verrou de l'instantané. Une seule
    // notification par changement d'état, la première publication incluse.
    if (ecu->on_connection_changed != NULL &&
        (!ecu->has_notified_connection ||
         ecu->last_notified_connected != data->connected)) {
        ecu->has_notified_connection = true;
        ecu->last_notified_connected = data->connected;
        ecu->on_connection_changed(ecu->connection_ctx, data->connected);
    }
}

// Attente avant reconnexion découpée en tranches : `mems_ecu_destroy` ne doit
// pas patienter une seconde entière quand l'ECU est absente.
static void delay_reconnect_interruptible(const mems_ecu_t *ecu) {
    uint32_t remaining = ecu->reconnect_delay_ms;
    while (ecu->running && remaining > 0) {
        uint32_t step = remaining > 50 ? 50 : remaining;
        TickType_t ticks = pdMS_TO_TICKS(step);
        if (ticks == 0) ticks = 1;
        vTaskDelay(ticks);
        remaining -= step;
    }
}

static void mems_ecu_task(void *arg) {
    mems_ecu_t *ecu = arg;
    TickType_t period = pdMS_TO_TICKS(ecu->poll_period_ms);
    if (period == 0) period = 1;
    TickType_t last_wake = xTaskGetTickCount();

    while (ecu->running) {
        if (!mems_session_is_connected(&ecu->session)) {
            ESP_LOGI(TAG, "connexion à l'ECU MEMS…");
            if (!mems_session_connect(&ecu->session)) {
                // Publier l'état déconnecté et temporiser avant nouvel essai.
                const ecu_data_t down = ecu_data_unavailable();
                publish(ecu, &down);
                delay_reconnect_interruptible(ecu);
                continue;
            }
            const uint8_t *id = mems_reader_ecu_id(&ecu->base_reader);
            ESP_LOGI(TAG, "ECU connectée (ID %02X %02X %02X %02X)", id[1],
                     id[2], id[3], id[4]);
            // Repartir d'une cadence neuve après le handshake (durée variable).
            last_wake = xTaskGetTickCount();
        }

        // Cycle : 0x80 à chaque fois, 0x7D un cycle sur N (cf. mems_session).
        ecu_data_t data;
        if (mems_session_poll(&ecu->session, &data, NULL)) {
            publish(ecu, &data);
        } else if (mems_session_is_connected(&ecu->session)) {
            // Échec isolé : on garde le dernier instantané et on retente.
            ESP_LOGD(TAG, "trame MEMS perdue (%u/%u)",
                     mems_session_poll_failures(&ecu->session),
                     MEMS_SESSION_MAX_POLL_FAILURES);
        } else {
            ESP_LOGW(TAG, "ECU absente ou trames expirées, reconnexion");
            // Ne pas conserver un snapshot marqué connecté après la perte.
            const ecu_data_t down = ecu_data_unavailable();
            publish(ecu, &down);
            continue; // reconnexion immédiate, sans attendre la période
        }

        // Cadence périodique réelle : la durée des échanges est absorbée dans
        // la période. En cas de dépassement (lectures expirées), on recale la
        // référence au lieu d'enchaîner des cycles en rafale pour rattraper.
        if (xTaskDelayUntil(&last_wake, period) == pdFALSE) {
            last_wake = xTaskGetTickCount();
        }
    }

    // This is the final operation that touches `ecu`; the owner waits for
    // this signal before releasing the UART, mutex, and allocation.
    xSemaphoreGive(ecu->stopped);
    vTaskDelete(NULL);
}

mems_ecu_t *mems_ecu_create(const mems_ecu_config_t *config) {
    if (config == NULL || (config->variant != MEMS_VARIANT_1_6 &&
                           config->variant != MEMS_VARIANT_1_9)) {
        return NULL;
    }

    mems_ecu_t *ecu = calloc(1, sizeof(*ecu));
    if (ecu == NULL) return NULL;

    ecu->poll_period_ms =
        config->poll_period_ms > 0 ? config->poll_period_ms : DEFAULT_POLL_MS;
    ecu->reconnect_delay_ms = config->reconnect_delay_ms > 0
                                  ? config->reconnect_delay_ms
                                  : DEFAULT_RECONNECT_MS;
    ecu->on_connection_changed = config->on_connection_changed;
    ecu->connection_ctx = config->connection_ctx;

    ecu->lock = xSemaphoreCreateMutex();
    ecu->stopped = xSemaphoreCreateBinary();
    if (ecu->lock == NULL || ecu->stopped == NULL) {
        if (ecu->stopped != NULL) vSemaphoreDelete(ecu->stopped);
        if (ecu->lock != NULL) vSemaphoreDelete(ecu->lock);
        free(ecu);
        return NULL;
    }

    ecu->kline = kline_uart_create(&config->kline);
    if (ecu->kline == NULL) {
        vSemaphoreDelete(ecu->stopped);
        vSemaphoreDelete(ecu->lock);
        free(ecu);
        return NULL;
    }

    // Chaîne de lecteurs : impl commune MEMS, éventuellement décorée pour la 1.9
    // (réveil 5 bauds). La session pilote l'interface résultante, sans connaître
    // la variante.
    kline_transport_t transport = kline_uart_transport(ecu->kline);
    mems_reader_init(&ecu->base_reader, transport, READ_TIMEOUT_MS);
    ecu_reader_t reader = mems_reader_interface(&ecu->base_reader);
    if (config->variant == MEMS_VARIANT_1_9) {
        mems19_reader_init(&ecu->reader19, reader, transport);
        reader = mems19_reader_interface(&ecu->reader19);
    }
    mems_session_init(&ecu->session, reader);
    ecu->snapshot = ecu_data_unavailable();
    return ecu;
}

bool mems_ecu_start(mems_ecu_t *ecu) {
    if (ecu == NULL) return false;
    if (ecu->task != NULL) return true;

    ecu->running = true;
    BaseType_t ok = xTaskCreate(mems_ecu_task, "mems_ecu", TASK_STACK, ecu,
                                TASK_PRIO, &ecu->task);
    if (ok != pdPASS) {
        ecu->running = false;
        ecu->task = NULL;
        return false;
    }
    return true;
}

void mems_ecu_destroy(mems_ecu_t *ecu) {
    if (ecu == NULL) return;

    // Demander l'arrêt et attendre la fin réelle de la tâche. Un délai fixe
    // pourrait libérer l'UART ou le mutex pendant un échange encore actif.
    ecu->running = false;
    if (ecu->task != NULL && ecu->stopped != NULL) {
        (void)xSemaphoreTake(ecu->stopped, portMAX_DELAY);
    }
    if (ecu->kline != NULL) kline_uart_destroy(ecu->kline);
    if (ecu->stopped != NULL) vSemaphoreDelete(ecu->stopped);
    if (ecu->lock != NULL) vSemaphoreDelete(ecu->lock);
    free(ecu);
}

bool mems_ecu_read(mems_ecu_t *ecu, ecu_data_t *out) {
    if (ecu == NULL || out == NULL) return false;
    if (xSemaphoreTake(ecu->lock, portMAX_DELAY) != pdTRUE) return false;
    const ecu_data_t snapshot = ecu->snapshot;
    const int64_t stamp_us = ecu->snapshot_us;
    const bool has_snapshot = ecu->has_snapshot;
    xSemaphoreGive(ecu->lock);

    // La fraîcheur ne concerne que les instantanés marqués connectés : une
    // publication déconnectée reste déconnectée, et l'absence de publication
    // (`has_snapshot == false`) n'est pas une mesure actuelle.
    const uint64_t now_us = (uint64_t)esp_timer_get_time();
    if (snapshot.connected &&
        !ecu_snapshot_is_fresh(has_snapshot, (uint64_t)stamp_us, now_us)) {
        *out = ecu_data_unavailable();
        return true;
    }

    *out = snapshot;
    return true;
}

static bool mems_ecu_source_read(void *context, ecu_data_t *out) {
    return mems_ecu_read(context, out);
}

ecu_source_t mems_ecu_source(mems_ecu_t *ecu) {
    return (ecu_source_t){.read = mems_ecu_source_read, .context = ecu};
}
