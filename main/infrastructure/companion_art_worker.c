#include "infrastructure/companion_art_worker.h"

#include <stddef.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "infrastructure/jpeg_cover.h"

static const char *TAG = "music_art";

// 10 Kio : tjpgd garde 4 Kio de tampon de travail et un JDEC sur la pile.
#define ART_WORKER_STACK    10240U
#define ART_WORKER_PRIORITY (tskIDLE_PRIORITY + 3)

static void *art_alloc(size_t size) {
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p != NULL) return p;
    return heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

static void art_free(void *p) {
    if (p != NULL) heap_caps_free(p);
}

// État singleton du worker : une seule pochette, dernier-gagnant des deux
// côtés (entrée JPEG en attente de décodage, sortie en attente de LVGL).
typedef struct {
    SemaphoreHandle_t lock;    // protège les créneaux entrée/sortie
    SemaphoreHandle_t wake;    // réveille la tâche
    SemaphoreHandle_t stopped; // signal de fin réelle avant purge
    TaskHandle_t task;
    bool running;
    bool has_in;
    companion_art_jpeg_t in;
    bool has_out;
    uint16_t *out;
    int out_width;
    int out_height;
} art_worker_t;

static art_worker_t s_worker;

static void art_worker_task(void *argument) {
    (void)argument;
    for (;;) {
        xSemaphoreTake(s_worker.wake, portMAX_DELAY);

        xSemaphoreTake(s_worker.lock, portMAX_DELAY);
        if (!s_worker.running) {
            xSemaphoreGive(s_worker.lock);
            break;
        }
        const bool has_job = s_worker.has_in;
        companion_art_jpeg_t job = s_worker.in;
        s_worker.has_in = false;
        s_worker.in.data = NULL;
        xSemaphoreGive(s_worker.lock);

        if (!has_job) continue;

        uint16_t *pixels = art_alloc((size_t)MUSIC_COVER_WIDTH *
                                     MUSIC_COVER_HEIGHT * sizeof(uint16_t));
        ESP_LOGI(
            TAG,
            "cover output allocation: bytes=%u ok=%d internal free=%u psram free=%u",
            (unsigned)(MUSIC_COVER_WIDTH * MUSIC_COVER_HEIGHT *
                       sizeof(uint16_t)),
            pixels != NULL,
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                              MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM |
                                              MALLOC_CAP_8BIT));
        const bool decoded =
            pixels != NULL &&
            jpeg_cover_decode_amber(job.data, job.length, MUSIC_COVER_WIDTH,
                                    MUSIC_COVER_HEIGHT, pixels);
        art_free(job.data);

        if (!decoded) {
            ESP_LOGW(TAG, "pochette JPEG rejetée (%u octets)",
                     (unsigned)job.length);
            art_free(pixels);
            continue;
        }

        ESP_LOGI(TAG, "cover JPEG decoded: bytes=%u output=%dx%d",
                 (unsigned)job.length, MUSIC_COVER_WIDTH, MUSIC_COVER_HEIGHT);
        xSemaphoreTake(s_worker.lock, portMAX_DELAY);
        if (!s_worker.running) {
            // Arrêt demandé pendant le décodage : ne rien publier.
            art_free(pixels);
        } else {
            art_free(s_worker.out); // image plus récente remplace l'ancienne
            s_worker.out = pixels;
            s_worker.out_width = MUSIC_COVER_WIDTH;
            s_worker.out_height = MUSIC_COVER_HEIGHT;
            s_worker.has_out = true;
        }
        xSemaphoreGive(s_worker.lock);
    }

    xSemaphoreGive(s_worker.stopped);
    vTaskDelete(NULL);
}

void companion_art_worker_start(void) {
    if (s_worker.task != NULL) return;
    s_worker.lock = xSemaphoreCreateMutex();
    s_worker.wake = xSemaphoreCreateBinary();
    s_worker.stopped = xSemaphoreCreateBinary();
    if (s_worker.lock == NULL || s_worker.wake == NULL ||
        s_worker.stopped == NULL) {
        ESP_LOGE(TAG, "sémaphores indisponibles, pochette désactivée");
        if (s_worker.lock != NULL) vSemaphoreDelete(s_worker.lock);
        if (s_worker.wake != NULL) vSemaphoreDelete(s_worker.wake);
        if (s_worker.stopped != NULL) vSemaphoreDelete(s_worker.stopped);
        s_worker.lock = NULL;
        s_worker.wake = NULL;
        s_worker.stopped = NULL;
        return;
    }
    s_worker.running = true;
    if (xTaskCreate(art_worker_task, "music_art", ART_WORKER_STACK, NULL,
                    ART_WORKER_PRIORITY, &s_worker.task) != pdPASS) {
        ESP_LOGE(TAG, "tâche indisponible, pochette désactivée");
        s_worker.task = NULL;
        s_worker.running = false;
        vSemaphoreDelete(s_worker.lock);
        vSemaphoreDelete(s_worker.wake);
        vSemaphoreDelete(s_worker.stopped);
        s_worker.lock = NULL;
        s_worker.wake = NULL;
        s_worker.stopped = NULL;
    }
}

bool companion_art_worker_submit(companion_art_jpeg_t *art) {
    if (art == NULL || art->data == NULL || art->length == 0) return false;
    if (s_worker.task == NULL || !s_worker.running) {
        ESP_LOGW(TAG, "cover submit rejected: worker unavailable");
        art_free(art->data);
        art->data = NULL;
        art->length = 0;
        return false;
    }
    xSemaphoreTake(s_worker.lock, portMAX_DELAY);
    art_free(s_worker.in.data); // dernier-gagnant : purge l'attente précédente
    s_worker.in = *art;
    s_worker.has_in = true;
    xSemaphoreGive(s_worker.lock);
    art->data = NULL;
    art->length = 0;
    xSemaphoreGive(s_worker.wake);
    return true;
}

bool companion_art_worker_take(uint16_t **pixels, int *width, int *height) {
    if (pixels == NULL || width == NULL || height == NULL ||
        s_worker.lock == NULL) {
        return false;
    }
    bool has = false;
    xSemaphoreTake(s_worker.lock, portMAX_DELAY);
    if (s_worker.has_out) {
        *pixels = s_worker.out;
        *width = s_worker.out_width;
        *height = s_worker.out_height;
        s_worker.out = NULL;
        s_worker.has_out = false;
        has = true;
    }
    xSemaphoreGive(s_worker.lock);
    return has;
}

void companion_art_worker_free(uint16_t *pixels) {
    art_free(pixels);
}

void companion_art_worker_stop(void) {
    if (s_worker.task == NULL) return;
    xSemaphoreTake(s_worker.lock, portMAX_DELAY);
    s_worker.running = false;
    art_free(s_worker.in.data);
    s_worker.in.data = NULL;
    s_worker.has_in = false;
    art_free(s_worker.out);
    s_worker.out = NULL;
    s_worker.has_out = false;
    xSemaphoreGive(s_worker.lock);
    xSemaphoreGive(s_worker.wake);
    xSemaphoreTake(s_worker.stopped, portMAX_DELAY);
    s_worker.task = NULL;
    vSemaphoreDelete(s_worker.lock);
    vSemaphoreDelete(s_worker.wake);
    vSemaphoreDelete(s_worker.stopped);
    s_worker.lock = NULL;
    s_worker.wake = NULL;
    s_worker.stopped = NULL;
}
