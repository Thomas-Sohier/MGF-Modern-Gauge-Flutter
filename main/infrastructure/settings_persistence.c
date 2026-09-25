#include "infrastructure/settings_persistence.h"

#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "infrastructure/settings_store.h"

static const char *TAG = "settings_persist";

#define PERSISTENCE_TASK_STACK    4096U
#define PERSISTENCE_TASK_PRIORITY 3U
#define PERSISTENCE_QUEUE_LENGTH  1U

// Message unique du worker : soit une demande de sauvegarde (stop == false),
// soit la sentinelle d'arrêt de destroy (stop == true). La file est bornée à 1
// et écrite par xQueueOverwrite : une nouvelle demande remplace la précédente
// (dernier-gagne), sans accumulation.
typedef struct {
    app_settings_t settings;
    uint32_t generation;
    bool stop;
} persistence_message_t;

struct settings_persistence_s {
    QueueHandle_t requests;    // 1 élément, dernier-gagne
    SemaphoreHandle_t lock;    // protège l'état ci-dessous (jamais pendant NVS)
    SemaphoreHandle_t stopped; // fin de tâche avant libération
    TaskHandle_t task;
    volatile bool running;

    app_settings_t acknowledged; // dernier snapshot réellement écrit
    bool ack_valid;
    uint32_t next_generation;      // dernier numéro de demande émis
    uint32_t completed_generation; // dernière demande traitée par le worker
};

static void settings_persistence_release(settings_persistence_t *persistence) {
    if (persistence == NULL) return;
    if (persistence->stopped != NULL) vSemaphoreDelete(persistence->stopped);
    if (persistence->lock != NULL) vSemaphoreDelete(persistence->lock);
    if (persistence->requests != NULL) vQueueDelete(persistence->requests);
    free(persistence);
}

// Publie le résultat d'une écriture. Le mutex n'est tenu que le temps de copier
// le snapshot : jamais pendant settings_store_save().
static void publish_result(settings_persistence_t *persistence,
                           const persistence_message_t *message, bool saved) {
    if (xSemaphoreTake(persistence->lock, portMAX_DELAY) != pdTRUE) return;
    if (saved) {
        persistence->acknowledged = message->settings;
        persistence->ack_valid = true;
    }
    // Même en échec, la demande est « traitée » : un échec ne doit jamais
    // acquitter son snapshot, mais il ne doit pas non plus bloquer
    // définitivement la génération.
    persistence->completed_generation = message->generation;
    xSemaphoreGive(persistence->lock);
}

static void settings_persistence_task(void *arg) {
    settings_persistence_t *persistence = arg;

    while (persistence->running) {
        persistence_message_t message;
        if (xQueueReceive(persistence->requests, &message, portMAX_DELAY) !=
            pdTRUE) {
            continue;
        }
        if (message.stop || !persistence->running) break;

        // Écriture NVS hors de tout mutex : elle peut être lente.
        const esp_err_t error = settings_store_save(&message.settings);
        if (error != ESP_OK) {
            ESP_LOGW(TAG, "préférences non enregistrées: %s",
                     esp_err_to_name(error));
        }
        publish_result(persistence, &message, error == ESP_OK);
    }

    // Dernière opération touchant `persistence` : destroy attend ce signal
    // avant de libérer la file, les sémaphores et l'allocation.
    xSemaphoreGive(persistence->stopped);
    vTaskDelete(NULL);
}

settings_persistence_t *settings_persistence_create(void) {
    settings_persistence_t *persistence = calloc(1, sizeof(*persistence));
    if (persistence == NULL) return NULL;

    persistence->requests =
        xQueueCreate(PERSISTENCE_QUEUE_LENGTH, sizeof(persistence_message_t));
    persistence->lock = xSemaphoreCreateMutex();
    persistence->stopped = xSemaphoreCreateBinary();
    if (persistence->requests == NULL || persistence->lock == NULL ||
        persistence->stopped == NULL) {
        ESP_LOGW(TAG, "ressources FreeRTOS indisponibles");
        settings_persistence_release(persistence);
        return NULL;
    }

    persistence->running = true;
    if (xTaskCreate(settings_persistence_task, "settings_persist",
                    PERSISTENCE_TASK_STACK, persistence,
                    PERSISTENCE_TASK_PRIORITY, &persistence->task) != pdPASS) {
        ESP_LOGW(TAG, "création de la tâche de persistance impossible");
        persistence->running = false;
        persistence->task = NULL;
        settings_persistence_release(persistence);
        return NULL;
    }
    return persistence;
}

bool settings_persistence_save(void *context, const app_settings_t *settings) {
    settings_persistence_t *persistence = context;
    if (persistence == NULL || settings == NULL ||
        !app_settings_is_valid(settings) || !persistence->running) {
        return false;
    }

    // Jamais de blocage depuis la tâche UI : mutex pris en 0 tick. Si le worker
    // publie un acquittement, on abandonne et le coordinateur réessaiera.
    if (xSemaphoreTake(persistence->lock, 0) != pdTRUE) return false;

    // true seulement si aucun snapshot plus récent n'est en file ou en cours :
    // sinon une écriture différente pourrait écraser l'acquittement après ce
    // retour. `completed_generation == next_generation` exprime exactement cela.
    if (persistence->ack_valid &&
        persistence->completed_generation == persistence->next_generation &&
        app_settings_equal(&persistence->acknowledged, settings)) {
        xSemaphoreGive(persistence->lock);
        return true;
    }

    const persistence_message_t message = {
        .settings = *settings,
        .generation = ++persistence->next_generation,
        .stop = false,
    };
    // File bornée à 1 : la nouvelle demande remplace toute demande en attente
    // (dernier-gagne). Sous le mutex pour que la génération reste cohérente
    // avec le contenu de la file. xQueueOverwrite ne bloque jamais.
    const bool queued =
        xQueueOverwrite(persistence->requests, &message) == pdPASS;
    xSemaphoreGive(persistence->lock);

    if (!queued) {
        ESP_LOGW(TAG, "demande de sauvegarde non mise en file");
    }
    return false;
}

void settings_persistence_destroy(settings_persistence_t *persistence) {
    if (persistence == NULL) return;

    // Les producteurs doivent déjà être arrêtés : aucun appel concurrent à
    // settings_persistence_save() ne doit être en cours.
    persistence->running = false;
    if (persistence->requests != NULL) {
        // Message d'arrêt dédié : remplace une éventuelle demande en attente,
        // sans importance à l'arrêt.
        const persistence_message_t stop = {.stop = true};
        (void)xQueueOverwrite(persistence->requests, &stop);
    }

    // Join : ne jamais libérer la file ou le mutex pendant que la tâche écrit.
    if (persistence->task != NULL && persistence->stopped != NULL) {
        (void)xSemaphoreTake(persistence->stopped, portMAX_DELAY);
    }
    settings_persistence_release(persistence);
}
