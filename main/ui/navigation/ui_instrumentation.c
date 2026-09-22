#include "ui/navigation/ui_instrumentation.h"

#if MGF_UI_INSTRUMENTATION && defined(ESP_PLATFORM)

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#define LOG_INTERVAL_US 1000000

static const char *TAG = "mgf_ui_perf";
static uint64_t last_log_us;
static uint64_t total_us;
static uint32_t tick_count;
static uint32_t max_us;

uint64_t ui_instrumentation_begin(void) {
    return (uint64_t)esp_timer_get_time();
}

void ui_instrumentation_end(const char *page, uint64_t started_at) {
    const uint64_t now = (uint64_t)esp_timer_get_time();
    const uint32_t elapsed_us = now >= started_at
        ? (uint32_t)(now - started_at) : 0;
    total_us += elapsed_us;
    if (elapsed_us > max_us) max_us = elapsed_us;
    tick_count++;

    if (last_log_us == 0) last_log_us = now;
    if (now - last_log_us < LOG_INTERVAL_US) return;

    const uint32_t average_us = tick_count == 0
        ? 0 : (uint32_t)(total_us / tick_count);
    ESP_LOGI(TAG, "page=%s ticks=%u ui_us(avg/max)=%u/%u free_int=%u free_psram=%u",
             page != NULL ? page : "?", tick_count, average_us, max_us,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    last_log_us = now;
    total_us = 0;
    tick_count = 0;
    max_us = 0;
}

#endif
