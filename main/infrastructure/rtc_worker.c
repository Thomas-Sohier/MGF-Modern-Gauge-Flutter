#include "infrastructure/rtc_worker.h"

#include <stddef.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "domain/rtc_time.h"

static const char *TAG = "rtc_worker";

#define RTC_WORKER_TASK_STACK    4096U
#define RTC_WORKER_TASK_PRIORITY 3U
#define RTC_WORKER_RETRY_MS      5000U
#define RTC_WORKER_RETRY_US      ((int64_t)RTC_WORKER_RETRY_MS * 1000)
#define RTC_WORKER_PERIOD_MS     60000U
#define RTC_WORKER_QUEUE_LENGTH  1U

// Commande déposée par une tâche appelante. `submit_us` est l'horodatage
// monotone esp_timer au moment du submit : il permet de compenser le séjour en
// file avant l'écriture RTC. `stop` est la sentinelle d'arrêt : elle voyage
// dans la file pour réveiller et terminer la tâche sans drapeau partagé.
typedef struct {
    rtc_datetime_t utc;
    int64_t submit_us;
    bool stop;
} rtc_command_t;

struct rtc_worker_s {
    rtc_t *rtc; // emprunté : jamais détruit par le worker

    QueueHandle_t commands;       // 1 élément, dernier-gagne (xQueueOverwrite)
    SemaphoreHandle_t stopped;    // signal de fin de tâche avant free()
    SemaphoreHandle_t state_lock; // protège l'état partagé ci-dessous
    TaskHandle_t task;

    // Etat partagé, protégé par state_lock et JAMAIS tenu pendant l'I2C.
    // Dès qu'une commande téléphone est acceptée, le système devient
    // prioritaire pour toute la session : le recalage périodique n'écrit plus
    // jamais l'horloge système.
    bool phone_seen;
};

static bool datetime_equal(const rtc_datetime_t *a, const rtc_datetime_t *b) {
    return a->year == b->year && a->month == b->month && a->day == b->day &&
           a->weekday == b->weekday && a->hour == b->hour &&
           a->minute == b->minute && a->second == b->second;
}

static bool rtc_worker_phone_seen(rtc_worker_t *worker) {
    xSemaphoreTake(worker->state_lock, portMAX_DELAY);
    const bool seen = worker->phone_seen;
    xSemaphoreGive(worker->state_lock);
    return seen;
}

// Applique une commande téléphone : écriture RTC hors LVGL, avec la valeur
// compensée du temps passé en file. Le worker ne touche JAMAIS l'horloge
// système : elle est mise à l'heure par l'appelant (submit puis settimeofday),
// ce qui garantit qu'aucune valeur RTC ne peut écraser une sync téléphone plus
// récente. `*last_applied`/`*has_applied` sont l'état privé de la tâche :
// déduplication d'un renvoi identique, comparée à l'UTC brut réellement écrit.
// Retourne false si l'écriture a échoué : l'appelant décide du réessai borné.
static bool rtc_worker_apply_command(rtc_worker_t *worker,
                                     const rtc_command_t *command,
                                     rtc_datetime_t *last_applied,
                                     bool *has_applied) {
    int64_t unix_seconds = 0;
    if (!rtc_datetime_to_unix(&command->utc, &unix_seconds)) {
        ESP_LOGW(TAG, "commande RTC hors plage, ignorée");
        return false;
    }

    // La commande décrit l'heure au moment du submit ; le téléphone a déjà
    // réglé l'horloge système immédiatement. On ajoute le délai écoulé pour
    // que la RTC rattrape le temps passé en file.
    const int64_t elapsed_us = esp_timer_get_time() - command->submit_us;
    if (elapsed_us > 0) unix_seconds += elapsed_us / 1000000;

    rtc_datetime_t adjusted;
    if (!rtc_datetime_from_unix(unix_seconds, &adjusted)) {
        ESP_LOGW(TAG, "commande RTC compensée invalide, ignorée");
        return false;
    }

    const rtc_result_t result = rtc_set(worker->rtc, &adjusted);
    if (result != RTC_OK) {
        ESP_LOGW(TAG, "écriture RTC depuis le téléphone échouée: %s",
                 rtc_result_name(result));
        return false;
    }

    // Mémorise l'UTC brut de la commande : un renvoi exactement identique du
    // téléphone sera dédupliqué, une heure différente non.
    *last_applied = command->utc;
    *has_applied = true;
    return true;
}

// Recalage périodique depuis la RTC. La lecture I2C se fait HORS verrou ; la
// décision d'écrire l'horloge système se prend SOUS verrou, avec settimeofday
// (appel court, pas d'I/O). `*synced` ne passe à vrai qu'après une lecture
// valide : un échec laisse l'horloge système avancer sans la toucher.
static void rtc_worker_recalibrate(rtc_worker_t *worker, bool *synced) {
    rtc_datetime_t utc;
    const rtc_result_t result = rtc_read(worker->rtc, &utc);
    if (result != RTC_OK) {
        ESP_LOGW(TAG, "lecture RTC échouée: %s (horloge système conservée)",
                 rtc_result_name(result));
        return;
    }

    int64_t unix_seconds = 0;
    if (!rtc_datetime_to_unix(&utc, &unix_seconds)) return;

    // Décision atomique avec le submit : si une heure téléphone a été publiée
    // (phone_seen), la RTC ne doit plus jamais toucher le système. Le verrou
    // est tenu pendant settimeofday pour qu'un submit concurrent (qui publie
    // phone_seen et met en file sous le même verrou) soit sérialisé : dans tous
    // les cas de figure, l'heure téléphone est appliquée en dernier.
    xSemaphoreTake(worker->state_lock, portMAX_DELAY);
    if (!worker->phone_seen) {
        struct timeval now = {
            .tv_sec = (time_t)unix_seconds,
            .tv_usec = 0,
        };
        if (settimeofday(&now, NULL) != 0) {
            ESP_LOGW(TAG, "settimeofday depuis la RTC échoué");
        } else {
            if (!*synced) {
                ESP_LOGI(TAG, "horloge système initialisée depuis la RTC");
            }
            *synced = true;
        }
    }
    xSemaphoreGive(worker->state_lock);
}

static void rtc_worker_task(void *arg) {
    rtc_worker_t *worker = arg;
    bool synced = false;
    bool has_pending = false;
    rtc_command_t pending = {0};
    int64_t next_retry_us = 0; // réessai borné d'une écriture en échec
    int64_t next_read_us = esp_timer_get_time(); // première lecture au boot
    // Dédup privée à la tâche (jamais partagée, jamais lue par le submit) :
    // dernier UTC brut réellement écrit avec succès.
    bool has_applied = false;
    rtc_datetime_t last_applied = {0};

    // Boucle unique : l'arrêt arrive comme sentinelle dans la file, ce qui
    // réveille xQueueReceive sans drapeau `running` partagé (data race).
    for (;;) {
        const int64_t now_us = esp_timer_get_time();
        // Après une première heure téléphone, plus aucune lecture périodique de
        // la RTC : seules les écritures + réessais restent actifs.
        const bool periodic = !rtc_worker_phone_seen(worker);
        int64_t deadline_us = periodic ? next_read_us : INT64_MAX;
        if (has_pending && next_retry_us < deadline_us) {
            deadline_us = next_retry_us;
        }

        TickType_t wait = portMAX_DELAY;
        if (deadline_us != INT64_MAX) {
            int64_t wait_us = deadline_us - now_us;
            if (wait_us < 0) wait_us = 0;
            // Arrondi au tick supérieur : pdMS_TO_TICKS peut tronquer à 0, un
            // timeout nul ferait tourner la boucle à vide. Minimum 1 tick.
            wait = pdMS_TO_TICKS((uint32_t)((wait_us + 999) / 1000));
            if (wait == 0) wait = 1;
        }

        rtc_command_t command;
        const bool queued =
            xQueueReceive(worker->commands, &command, wait) == pdTRUE;
        if (queued && command.stop) break;

        if (queued) {
            // Dernier-gagne : une nouvelle commande remplace le réessai
            // éventuellement en attente.
            if (has_applied && datetime_equal(&last_applied, &command.utc)) {
                // Déjà écrite avec succès (même UTC brut) : acquittée sans I2C.
                has_pending = false;
            } else if (rtc_worker_apply_command(worker, &command,
                                                &last_applied, &has_applied)) {
                has_pending = false;
            } else {
                pending = command;
                has_pending = true;
                next_retry_us = esp_timer_get_time() + RTC_WORKER_RETRY_US;
            }
        } else if (has_pending && esp_timer_get_time() >= next_retry_us) {
            // Réessai borné (5 s), jamais de boucle active.
            if (has_applied && datetime_equal(&last_applied, &pending.utc)) {
                has_pending = false;
            } else if (rtc_worker_apply_command(worker, &pending, &last_applied,
                                                &has_applied)) {
                has_pending = false;
            } else {
                next_retry_us = esp_timer_get_time() + RTC_WORKER_RETRY_US;
            }
        }

        const int64_t after_us = esp_timer_get_time();
        // Re-vérifie phone_seen juste avant l'I2C : dès la première commande
        // téléphone, plus aucune lecture périodique n'est lancée. La lecture
        // elle-même reste protégée par le verrou pris dans recalibrate, donc une
        // commande concurrente ne peut jamais être écrasée.
        if (!has_pending && after_us >= next_read_us &&
            !rtc_worker_phone_seen(worker)) {
            rtc_worker_recalibrate(worker, &synced);
            const uint32_t period_ms =
                synced ? RTC_WORKER_PERIOD_MS : RTC_WORKER_RETRY_MS;
            next_read_us = after_us + (int64_t)period_ms * 1000;
        }
    }

    // Dernière opération touchant `worker` : le propriétaire attend ce signal
    // avant de libérer la file, les sémaphores et l'allocation.
    xSemaphoreGive(worker->stopped);
    vTaskDelete(NULL);
}

static void rtc_worker_release(rtc_worker_t *worker) {
    if (worker == NULL) return;
    if (worker->state_lock != NULL) vSemaphoreDelete(worker->state_lock);
    if (worker->stopped != NULL) vSemaphoreDelete(worker->stopped);
    if (worker->commands != NULL) vQueueDelete(worker->commands);
    free(worker);
}

rtc_worker_t *rtc_worker_start(rtc_t *borrowed) {
    if (borrowed == NULL) return NULL;

    rtc_worker_t *worker = calloc(1, sizeof(*worker));
    if (worker == NULL) return NULL;
    worker->rtc = borrowed;
    worker->commands =
        xQueueCreate(RTC_WORKER_QUEUE_LENGTH, sizeof(rtc_command_t));
    worker->stopped = xSemaphoreCreateBinary();
    worker->state_lock = xSemaphoreCreateMutex();
    if (worker->commands == NULL || worker->stopped == NULL ||
        worker->state_lock == NULL) {
        rtc_worker_release(worker);
        return NULL;
    }

    if (xTaskCreate(rtc_worker_task, "rtc_worker", RTC_WORKER_TASK_STACK,
                    worker, RTC_WORKER_TASK_PRIORITY,
                    &worker->task) != pdPASS) {
        worker->task = NULL;
        rtc_worker_release(worker);
        return NULL;
    }
    return worker;
}

bool rtc_worker_submit(rtc_worker_t *worker, const rtc_datetime_t *utc) {
    if (worker == NULL || utc == NULL || !rtc_datetime_is_valid(utc)) {
        return false;
    }

    const rtc_command_t command = {
        .utc = *utc,
        .submit_us = esp_timer_get_time(),
        .stop = false,
    };
    // phone_seen et la mise en file sont publiés SOUS LE MÊME VERROU : le
    // recalage périodique qui observe phone_seen=true est donc forcément
    // postérieur à l'acceptation de la commande (pas de fenêtre TOCTOU). Aucune
    // dédup ici (elle est faite par la tâche, au moment de traiter la commande).
    // File bornée à 1 : la nouvelle commande remplace toute commande en attente
    // (dernier-gagne), jamais d'accumulation.
    xSemaphoreTake(worker->state_lock, portMAX_DELAY);
    worker->phone_seen = true;
    const bool queued =
        xQueueOverwrite(worker->commands, &command) == pdPASS;
    xSemaphoreGive(worker->state_lock);
    return queued;
}

void rtc_worker_stop(rtc_worker_t *worker) {
    if (worker == NULL) return;

    // Réveille la tâche bloquée sur la file avec la sentinelle d'arrêt :
    // xQueueOverwrite remplace toute commande en attente, sans importance à
    // l'arrêt. Plus aucun producteur ne doit appeler submit après ce point.
    const rtc_command_t stop_command = {
        .utc = {0},
        .submit_us = 0,
        .stop = true,
    };
    (void)xQueueOverwrite(worker->commands, &stop_command);

    // Join : ne jamais libérer pendant qu'une transaction I2C est en cours.
    if (worker->task != NULL && worker->stopped != NULL) {
        (void)xSemaphoreTake(worker->stopped, portMAX_DELAY);
    }
    rtc_worker_release(worker);
}
