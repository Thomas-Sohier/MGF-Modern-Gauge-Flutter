// Tests hôte de la source ECU réelle (main/infrastructure/mems_ecu.c).
//
// La tâche de fond est réellement exécutée (un fil POSIX la porte), ce qui
// couvre le cycle de vie complet : validation des arguments, instantané initial,
// connexion MEMS 1.6 / 1.9, fraîcheur de l'instantané, perte de liaison et
// arrêt propre (`mems_ecu_destroy` attend le signal `stopped`).
//
// Les doublures FreeRTOS/K-line vivent ici et non dans `test/mocks` : elles
// laissent scripter le transport K-line octet par octet et n'imposent aucun
// pilote UART/GPIO bas niveau.

// `usleep`/`nanosleep` : la compilation hôte est stricte (`-std=c11`), les
// déclarations POSIX doivent donc être demandées explicitement.
#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "infrastructure/kline_uart_esp32.h"
#include "infrastructure/mems_ecu.h"

#include "domain/mems_protocol.h"

// `pdPASS` n'est pas défini par l'en-tête FreeRTOS factice.
#ifndef pdPASS
#define pdPASS 1
#endif

static void check(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        _Exit(1);
    }
}

// --- Sémaphores FreeRTOS factices -------------------------------------------
// `SemaphoreHandle_t` vient de `test/mocks/freertos/semphr.h` (pointeur sur
// `struct mock_semaphore`). Un mutex démarre disponible ; une sémaphore binaire
// démarre vide.

struct mock_semaphore {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool binary;
    bool signaled;
    bool deleted;
    int gives;
    int takes;
};

static int g_mutex_created;
static int g_mutex_deleted;
static int g_binary_created;
static int g_binary_deleted;
static int g_binary_gives;
static int g_binary_takes;

static struct mock_semaphore *semaphore_new(bool binary) {
    struct mock_semaphore *sem = calloc(1, sizeof(*sem));
    check(sem != NULL, "allocation de sémaphore");
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
    sem->binary = binary;
    sem->signaled = !binary; // un mutex est disponible à la création
    if (binary) {
        g_binary_created++;
    } else {
        g_mutex_created++;
    }
    return sem;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    return semaphore_new(false);
}

SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    return semaphore_new(true);
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout) {
    if (semaphore == NULL) return pdFALSE;
    pthread_mutex_lock(&semaphore->mutex);
    if (!semaphore->signaled) {
        if (timeout != portMAX_DELAY) {
            pthread_mutex_unlock(&semaphore->mutex);
            return pdFALSE;
        }
        while (!semaphore->signaled) {
            pthread_cond_wait(&semaphore->cond, &semaphore->mutex);
        }
    }
    semaphore->signaled = false;
    semaphore->takes++;
    if (semaphore->binary) g_binary_takes++;
    pthread_mutex_unlock(&semaphore->mutex);
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore) {
    if (semaphore == NULL) return pdFALSE;
    pthread_mutex_lock(&semaphore->mutex);
    semaphore->signaled = true;
    semaphore->gives++;
    if (semaphore->binary) g_binary_gives++;
    pthread_cond_broadcast(&semaphore->cond);
    pthread_mutex_unlock(&semaphore->mutex);
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t semaphore) {
    if (semaphore == NULL) return;
    // L'objet n'est pas libéré : la tâche qui vient de signaler `stopped` doit
    // pouvoir terminer sans toucher à de la mémoire rendue au tas.
    semaphore->deleted = true;
    if (semaphore->binary) {
        g_binary_deleted++;
    } else {
        g_mutex_deleted++;
    }
}

// --- Tâches FreeRTOS factices ----------------------------------------------
// `xTaskCreate` capture la fonction et son argument ; le test décide quand
// l'exécuter (fil dédié). Aucun ordonnanceur n'est simulé : les délais font
// seulement avancer un compteur de ticks.

typedef unsigned UBaseType_t;
typedef void (*TaskFunction_t)(void *);
typedef struct host_task *TaskHandle_t;

struct host_task {
    TaskFunction_t fn;
    void *arg;
};

static struct host_task *g_task;
static int g_task_create_calls;
static int g_vtask_delay_calls;
static TickType_t g_vtask_delay_max_ticks;
static bool g_task_exited;
static volatile TickType_t g_ticks;

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack,
                       void *arg, UBaseType_t priority, TaskHandle_t *out) {
    (void)name;
    (void)stack;
    (void)priority;
    if (fn == NULL || out == NULL) return pdFALSE;
    struct host_task *task = calloc(1, sizeof(*task));
    if (task == NULL) return pdFALSE;
    task->fn = fn;
    task->arg = arg;
    *out = task;
    g_task = task;
    g_task_create_calls++;
    return pdPASS;
}

void vTaskDelete(TaskHandle_t task) {
    (void)task;
    g_task_exited = true;
}

TickType_t xTaskGetTickCount(void) {
    return g_ticks++;
}

BaseType_t xTaskDelayUntil(TickType_t *previous, TickType_t increment) {
    if (previous != NULL) *previous += increment;
    g_ticks += increment;
    sched_yield();
    return pdTRUE;
}

void vTaskDelay(TickType_t ticks) {
    g_vtask_delay_calls++;
    if (ticks > g_vtask_delay_max_ticks) g_vtask_delay_max_ticks = ticks;
    g_ticks += ticks > 0 ? ticks : 1;
    sched_yield();
}

// --- Horloge monotone factice ---------------------------------------------
// Seul le test avance `g_now_us` : la fraîcheur d'un instantané est donc
// entièrement déterministe.

static int64_t g_now_us;

int64_t esp_timer_get_time(void) {
    return g_now_us;
}

// --- UART K-line factice ---------------------------------------------------
// `kline_uart_create` renvoie une instance opaque dont le transport est le
// script de ce fichier : la tâche MEMS dialogue donc avec une ECU simulée.

struct kline_uart_s {
    int unused;
};

static struct kline_uart_s g_kline;
static kline_uart_config_t g_last_kline_config;
static int g_kline_create_calls;
static int g_kline_destroy_calls;
static bool g_kline_create_fail;

// --- Transport K-line scriptable ------------------------------------------

#define MOCK_RESP_CAPACITY 256

// Trames de référence du projet Go `andrewdjackson/rosco` (mêmes fixtures que
// `test/test_mems.c`) : régime décodé à 1189 tr/min.
static const uint8_t FRAME_80[MEMS_FRAME_80_LEN] = {
    0x80, 0x1c, 0x04, 0xa5, 0x4b, 0xff, 0x4c, 0xff, 0x31, 0x82,
    0x22, 0x00, 0x20, 0x01, 0x00, 0x00, 0x00, 0x20, 0x84, 0x78,
    0x00, 0x1d, 0x00, 0x44, 0x06, 0x59, 0x10, 0x00, 0x00,
};
static const uint8_t FRAME_7D[MEMS_FRAME_7D_LEN] = {
    0x7d, 0x20, 0x10, 0x14, 0xff, 0x92, 0x40, 0x57, 0xff, 0xff, 0x01,
    0x00, 0x80, 0x64, 0x00, 0xff, 0x64, 0xff, 0xff, 0x30, 0x80, 0x80,
    0x0e, 0xff, 0x16, 0x80, 0x1b, 0x00, 0x22, 0x00, 0x31, 0xc0, 0x1f,
};

typedef struct {
    pthread_mutex_t lock; // protège la file de réponses et les compteurs
    pthread_mutex_t gate_mutex;
    pthread_cond_t gate_cond;
    uint8_t out[MOCK_RESP_CAPACITY];
    size_t out_len;
    size_t out_pos;
    bool silent;      // ECU absente : aucune réponse, pas même l'écho
    bool gate_closed; // une lecture attend l'autorisation du test
    bool gate_blocked;
    int connect_attempts; // handshakes INIT_A reçus
    int n80;
    int n7d;
    int wake_calls;
    uint8_t wake_addr;
} transport_mock_t;

static transport_mock_t g_transport;

static void transport_init(void) {
    pthread_mutex_init(&g_transport.lock, NULL);
    pthread_mutex_init(&g_transport.gate_mutex, NULL);
    pthread_cond_init(&g_transport.gate_cond, NULL);
}

static void enqueue_locked(transport_mock_t *m, const uint8_t *data,
                           size_t len) {
    check(len <= sizeof(m->out) - m->out_len,
          "capacité de la file de réponses");
    memcpy(m->out + m->out_len, data, len);
    m->out_len += len;
}

static int mock_write(void *ctx, const uint8_t *buf, size_t len) {
    transport_mock_t *m = ctx;
    if (len == 0) return 0;
    const uint8_t cmd = buf[len - 1];
    pthread_mutex_lock(&m->lock);
    if (m->silent) {
        if (cmd == MEMS_CMD_INIT_A) m->connect_attempts++;
        pthread_mutex_unlock(&m->lock);
        return (int)len;
    }
    switch (cmd) {
    case MEMS_CMD_INIT_A:
        m->connect_attempts++;
        enqueue_locked(m, &cmd, 1);
        break;
    case MEMS_CMD_INIT_B:
        enqueue_locked(m, &cmd, 1);
        break;
    case MEMS_CMD_HEARTBEAT: {
        const uint8_t response[2] = {cmd, 0x00};
        enqueue_locked(m, response, sizeof(response));
        break;
    }
    case MEMS_CMD_INIT_ECU_ID: {
        const uint8_t response[5] = {cmd, 0x99, 0x00, 0x02, 0x03};
        enqueue_locked(m, response, sizeof(response));
        break;
    }
    case MEMS_CMD_DATA_80:
        m->n80++;
        enqueue_locked(m, FRAME_80, sizeof(FRAME_80));
        break;
    case MEMS_CMD_DATA_7D:
        m->n7d++;
        enqueue_locked(m, FRAME_7D, sizeof(FRAME_7D));
        break;
    default:
        break;
    }
    pthread_mutex_unlock(&m->lock);
    return (int)len;
}

static int mock_read(void *ctx, uint8_t *buf, size_t len, uint32_t timeout_ms) {
    (void)timeout_ms;
    transport_mock_t *m = ctx;

    pthread_mutex_lock(&m->gate_mutex);
    if (m->gate_closed) {
        m->gate_blocked = true;
        pthread_cond_broadcast(&m->gate_cond);
        while (m->gate_closed) {
            pthread_cond_wait(&m->gate_cond, &m->gate_mutex);
        }
        m->gate_blocked = false;
    }
    pthread_mutex_unlock(&m->gate_mutex);

    pthread_mutex_lock(&m->lock);
    int result = 0;
    const size_t available = m->out_len - m->out_pos;
    if (available > 0) {
        const size_t count = available < len ? available : len;
        memcpy(buf, m->out + m->out_pos, count);
        m->out_pos += count;
        if (m->out_pos == m->out_len) m->out_pos = m->out_len = 0;
        result = (int)count;
    }
    pthread_mutex_unlock(&m->lock);
    return result;
}

static void mock_flush(void *ctx) {
    transport_mock_t *m = ctx;
    pthread_mutex_lock(&m->lock);
    m->out_len = 0;
    m->out_pos = 0;
    pthread_mutex_unlock(&m->lock);
}

static int mock_wake_up(void *ctx, uint8_t ecu_address) {
    transport_mock_t *m = ctx;
    pthread_mutex_lock(&m->lock);
    m->wake_calls++;
    m->wake_addr = ecu_address;
    pthread_mutex_unlock(&m->lock);
    return 0;
}

static void gate_close(void) {
    pthread_mutex_lock(&g_transport.gate_mutex);
    g_transport.gate_closed = true;
    pthread_mutex_unlock(&g_transport.gate_mutex);
}

static void gate_open(void) {
    pthread_mutex_lock(&g_transport.gate_mutex);
    g_transport.gate_closed = false;
    pthread_cond_broadcast(&g_transport.gate_cond);
    pthread_mutex_unlock(&g_transport.gate_mutex);
}

static bool gate_blocked(void) {
    pthread_mutex_lock(&g_transport.gate_mutex);
    const bool blocked = g_transport.gate_blocked;
    pthread_mutex_unlock(&g_transport.gate_mutex);
    return blocked;
}

static int transport_connect_attempts(void) {
    pthread_mutex_lock(&g_transport.lock);
    const int value = g_transport.connect_attempts;
    pthread_mutex_unlock(&g_transport.lock);
    return value;
}

static int transport_wake_calls(void) {
    pthread_mutex_lock(&g_transport.lock);
    const int value = g_transport.wake_calls;
    pthread_mutex_unlock(&g_transport.lock);
    return value;
}

static uint8_t transport_wake_addr(void) {
    pthread_mutex_lock(&g_transport.lock);
    const uint8_t value = g_transport.wake_addr;
    pthread_mutex_unlock(&g_transport.lock);
    return value;
}

kline_uart_t *kline_uart_create(const kline_uart_config_t *config) {
    g_kline_create_calls++;
    if (g_kline_create_fail || config == NULL) return NULL;
    g_last_kline_config = *config;
    return &g_kline;
}

void kline_uart_destroy(kline_uart_t *kline) {
    if (kline == &g_kline) g_kline_destroy_calls++;
}

kline_transport_t kline_uart_transport(kline_uart_t *kline) {
    (void)kline;
    return (kline_transport_t){
        .write = mock_write,
        .read = mock_read,
        .flush = mock_flush,
        .wake_up = mock_wake_up,
        .ctx = &g_transport,
    };
}

// --- Harnais ---------------------------------------------------------------

#define MAX_NOTIFICATIONS 8

static bool g_notifications[MAX_NOTIFICATIONS];
static int g_notification_count;
static int g_notification_ctx_ok;
static int g_notification_ctx;

static void on_connection_changed(void *ctx, bool connected) {
    if (ctx == &g_notification_ctx) g_notification_ctx_ok++;
    if (g_notification_count < MAX_NOTIFICATIONS) {
        g_notifications[g_notification_count++] = connected;
    }
}

static mems_ecu_t *g_ecu;
static pthread_t g_thread;
static bool g_thread_started;

static void mocks_reset(void) {
    g_mutex_created = 0;
    g_mutex_deleted = 0;
    g_binary_created = 0;
    g_binary_deleted = 0;
    g_binary_gives = 0;
    g_binary_takes = 0;

    g_task = NULL;
    g_task_create_calls = 0;
    g_vtask_delay_calls = 0;
    g_vtask_delay_max_ticks = 0;
    g_task_exited = false;
    g_ticks = 0;
    g_now_us = 0;

    g_kline_create_calls = 0;
    g_kline_destroy_calls = 0;
    g_kline_create_fail = false;

    g_notification_count = 0;
    g_notification_ctx_ok = 0;

    g_transport.out_len = 0;
    g_transport.out_pos = 0;
    g_transport.silent = false;
    g_transport.connect_attempts = 0;
    g_transport.n80 = 0;
    g_transport.n7d = 0;
    g_transport.wake_calls = 0;
    g_transport.wake_addr = 0;
    gate_open();
}

static mems_ecu_config_t make_config(mems_variant_t variant) {
    const mems_ecu_config_t config = {
        .kline =
            {
                .uart_num = UART_NUM_1,
                .tx_gpio = 40,
                .rx_gpio = 38,
                .baud_rate = 9600,
                .local_echo = true,
            },
        .variant = variant,
        .poll_period_ms = 1,
        .reconnect_delay_ms = 120,
        .on_connection_changed = on_connection_changed,
        .connection_ctx = &g_notification_ctx,
    };
    return config;
}

typedef bool (*wait_cb_t)(void);

static bool wait_until(wait_cb_t condition, uint32_t timeout_ms) {
    const uint32_t steps = timeout_ms * 5u;    // 200 µs par étape
    const struct timespec pause = {0, 200000}; // 200 µs
    for (uint32_t i = 0; i < steps; i++) {
        if (condition()) return true;
        nanosleep(&pause, NULL);
    }
    return condition();
}

static void *task_thread_entry(void *unused) {
    (void)unused;
    g_task->fn(g_task->arg);
    return NULL;
}

static void run_task(void) {
    check(g_task != NULL, "tâche capturée par xTaskCreate");
    check(pthread_create(&g_thread, NULL, task_thread_entry, NULL) == 0,
          "démarrage du fil de tâche");
    g_thread_started = true;
}

static void join_task(void) {
    if (!g_thread_started) return;
    pthread_join(g_thread, NULL);
    g_thread_started = false;
}

static bool wait_connected(void) {
    ecu_data_t data;
    return mems_ecu_read(g_ecu, &data) && data.connected;
}

static bool wait_first_notification(void) {
    return g_notification_count >= 1;
}

static bool wait_link_lost(void) {
    return g_notification_count >= 2 && !g_notifications[1];
}

static bool wait_reconnect_attempt(void) {
    return transport_connect_attempts() >= 2;
}

static bool wait_gate_blocked(void) {
    return gate_blocked();
}

// --- Tests -----------------------------------------------------------------

// 1. Validation des arguments de création.
static void test_create_validation(void) {
    mocks_reset();
    check(mems_ecu_create(NULL) == NULL, "configuration NULL rejetée");

    mems_ecu_config_t config = make_config((mems_variant_t)7);
    check(mems_ecu_create(&config) == NULL, "variante inconnue rejetée");

    config = make_config(MEMS_VARIANT_1_6);
    g_kline_create_fail = true;
    check(mems_ecu_create(&config) == NULL,
          "échec de kline_uart_create propagé");
    check(g_kline_create_calls == 1, "kline_uart_create tenté une seule fois");
    check(g_mutex_deleted == 1 && g_binary_deleted == 1,
          "sémaphores relâchés quand l'UART échoue");

    g_kline_create_fail = false;
    mems_ecu_t *ecu = mems_ecu_create(&config);
    check(ecu != NULL, "création nominale");
    check(g_kline_create_calls == 2, "UART recréé");
    check(g_mutex_created == 2 && g_binary_created == 2,
          "un verrou et un sémaphore d'arrêt par instance");
    check(g_last_kline_config.tx_gpio == 40 &&
              g_last_kline_config.rx_gpio == 38,
          "brochage K-line transmis au pilote");
    check(g_last_kline_config.local_echo, "écho local transmis au pilote");
    check(g_last_kline_config.baud_rate == 9600, "9600 bauds transmis");

    mems_ecu_destroy(ecu);
    check(g_kline_destroy_calls == 1, "UART libéré par destroy");
    check(g_mutex_deleted == 2 && g_binary_deleted == 2,
          "les deux sémaphores sont libérés");
}

// 2. Aucune mesure avant le premier poll.
static void test_initial_snapshot(void) {
    mocks_reset();
    const mems_ecu_config_t config = make_config(MEMS_VARIANT_1_6);
    mems_ecu_t *ecu = mems_ecu_create(&config);
    check(ecu != NULL, "instance créée sans tâche");

    ecu_data_t out;
    memset(&out, 0xA5, sizeof(out));
    check(mems_ecu_read(ecu, &out), "lecture possible avant le premier poll");
    check(!out.connected, "aucune connexion avant le premier poll");
    check(isnan(out.rpm), "aucune mesure avant le premier poll");

    check(!mems_ecu_read(NULL, &out), "instance NULL rejetée en lecture");
    check(!mems_ecu_read(ecu, NULL), "sortie NULL rejetée en lecture");
    check(g_notification_count == 0, "aucune notification sans tâche");

    const ecu_source_t source = mems_ecu_source(ecu);
    check(source.read != NULL && source.context == ecu,
          "adaptateur ecu_source exposé");
    check(source.read(source.context, &out) && !out.connected,
          "la source relaie l'instantané indisponible");

    mems_ecu_destroy(NULL); // no-op
    mems_ecu_destroy(ecu);
    check(g_kline_destroy_calls == 1, "UART libéré sans tâche démarrée");
}

// 3. Cycle nominal (MEMS 1.6 et 1.9) : connexion, poll, notification.
static void test_nominal_cycle(mems_variant_t variant) {
    mocks_reset();
    const mems_ecu_config_t config = make_config(variant);
    mems_ecu_t *ecu = mems_ecu_create(&config);
    check(ecu != NULL, "instance créée");
    g_ecu = ecu;

    check(mems_ecu_start(ecu), "tâche démarrée");
    check(mems_ecu_start(ecu), "démarrage idempotent");
    check(g_task_create_calls == 1, "une seule tâche créée");

    run_task();
    check(wait_until(wait_connected, 2000), "l'ECU se connecte");

    ecu_data_t out;
    check(mems_ecu_read(ecu, &out) && out.connected, "instantané connecté");
    check(out.rpm == 1189.0f, "régime décodé depuis la trame 0x80");

    check(wait_until(wait_first_notification, 1000), "connexion notifiée");
    check(g_notification_count == 1 && g_notifications[0],
          "une seule notification, connectée");
    check(g_notification_ctx_ok == 1, "contexte de notification transmis");

    if (variant == MEMS_VARIANT_1_9) {
        check(transport_wake_calls() >= 1, "réveil 5 bauds émis");
        check(transport_wake_addr() == MEMS_ECU_ADDRESS,
              "adresse de réveil 0x16");
    } else {
        check(transport_wake_calls() == 0, "aucun réveil en MEMS 1.6");
    }

    mems_ecu_destroy(ecu);
    join_task();
    check(g_kline_destroy_calls == 1, "UART libéré par destroy");
    check(g_binary_deleted == 1, "sémaphore stopped libéré");
    g_ecu = NULL;
}

// 4. Fraîcheur : au-delà de 1,5 s, l'instantané connecté devient indisponible.
static void test_snapshot_freshness(void) {
    mocks_reset();
    const mems_ecu_config_t config = make_config(MEMS_VARIANT_1_6);
    mems_ecu_t *ecu = mems_ecu_create(&config);
    check(ecu != NULL, "instance créée");
    g_ecu = ecu;
    check(mems_ecu_start(ecu), "tâche démarrée");
    run_task();
    check(wait_until(wait_connected, 2000), "connexion avant gel");

    // Geler la tâche dans une lecture : plus aucune publication, l'horodatage
    // de l'instantané cesse d'avancer.
    gate_close();
    check(wait_until(wait_gate_blocked, 2000), "la tâche attend une réponse");

    g_now_us = 1499999; // juste avant la borne
    ecu_data_t out;
    check(mems_ecu_read(ecu, &out) && out.connected && out.rpm == 1189.0f,
          "instantané encore frais avant la borne");

    g_now_us = 1500000; // borne exacte : périmé
    check(mems_ecu_read(ecu, &out), "lecture pendant la péremption");
    check(!out.connected, "instantané périmé présenté déconnecté");
    check(isnan(out.rpm), "instantané périmé présenté indisponible");
    check(g_notification_count == 1 && g_notifications[0],
          "la péremption ne produit aucune notification");

    gate_open();
    mems_ecu_destroy(ecu);
    join_task();
    g_ecu = NULL;
}

// 5. Perte de liaison après MEMS_SESSION_MAX_POLL_FAILURES échecs consécutifs.
static void test_link_loss(void) {
    mocks_reset();
    const mems_ecu_config_t config = make_config(MEMS_VARIANT_1_6);
    mems_ecu_t *ecu = mems_ecu_create(&config);
    check(ecu != NULL, "instance créée");
    g_ecu = ecu;
    check(mems_ecu_start(ecu), "tâche démarrée");
    run_task();
    check(wait_until(wait_connected, 2000), "connexion initiale");
    check(wait_until(wait_first_notification, 1000) && g_notifications[0],
          "connexion notifiée");

    // ECU débranchée : plus aucun écho, donc plus aucune trame.
    pthread_mutex_lock(&g_transport.lock);
    g_transport.silent = true;
    pthread_mutex_unlock(&g_transport.lock);
    mock_flush(&g_transport);

    check(wait_until(wait_link_lost, 3000), "perte de liaison notifiée");
    check(!g_notifications[1], "notification de déconnexion");

    ecu_data_t out;
    check(mems_ecu_read(ecu, &out), "lecture après perte de liaison");
    check(!out.connected, "instantané déconnecté publié");
    check(isnan(out.rpm), "mesures indisponibles après perte de liaison");

    check(wait_until(wait_reconnect_attempt, 3000),
          "reconnexion tentée après la perte");
    check(g_vtask_delay_calls >= 3,
          "attente de reconnexion découpée en tranches");
    check(g_vtask_delay_max_ticks <= pdMS_TO_TICKS(50),
          "aucune tranche d'attente au-delà de 50 ms");

    mems_ecu_destroy(ecu);
    join_task();
    check(g_kline_destroy_calls == 1, "UART libéré par destroy");
    g_ecu = NULL;
}

// 6. Arrêt propre : la tâche signale `stopped` avant la libération des
// ressources, y compris quand elle boucle sur des reconnexions.
static void test_destroy_signals_stopped(void) {
    mocks_reset();
    const mems_ecu_config_t config = make_config(MEMS_VARIANT_1_9);
    mems_ecu_t *ecu = mems_ecu_create(&config);
    check(ecu != NULL, "instance créée");
    g_ecu = ecu;
    check(mems_ecu_start(ecu), "tâche démarrée");

    // ECU absente dès le premier essai : la tâche alterne publication
    // déconnectée et attente de reconnexion.
    pthread_mutex_lock(&g_transport.lock);
    g_transport.silent = true;
    pthread_mutex_unlock(&g_transport.lock);

    run_task();
    check(wait_until(wait_reconnect_attempt, 3000), "reconnexions en cours");
    check(wait_until(wait_first_notification, 1000) && !g_notifications[0],
          "échec de connexion notifié");

    mems_ecu_destroy(ecu);
    join_task();

    check(g_binary_gives >= 1, "sémaphore stopped signalé par la tâche");
    check(g_binary_takes >= 1, "mems_ecu_destroy a attendu ce signal");
    check(g_binary_deleted == 1, "sémaphore stopped libéré");
    check(g_mutex_deleted == 1, "verrou de l'instantané libéré");
    check(g_kline_destroy_calls == 1, "kline_uart_destroy appelé");
    check(g_task_exited, "la tâche a terminé son corps");
    g_ecu = NULL;
}

int main(void) {
    transport_init();

    test_create_validation();
    test_initial_snapshot();
    test_nominal_cycle(MEMS_VARIANT_1_6);
    test_nominal_cycle(MEMS_VARIANT_1_9);
    test_snapshot_freshness();
    test_link_loss();
    test_destroy_signals_stopped();

    puts("mems_ecu: ok");
    return 0;
}
