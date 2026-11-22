#include "infrastructure/perf_bench.h"

#if MGF_PERF_BENCH && defined(ESP_PLATFORM)

#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifndef MGF_PERF_BENCH_PAGE_MS
#define MGF_PERF_BENCH_PAGE_MS 8000
#endif
#define MAX_TASKS 32

static const char *TAG = "mgf_bench";

typedef struct {
    uint32_t frames;
    uint64_t refr_us;  // REFR_START -> REFR_READY (rendu + sync + flush)
    uint64_t flush_us; // somme des flush (attente vsync comprise)
    uint32_t refr_max_us;
    uint32_t render_max_us; // refr - flush
    uint64_t dirty_px;
    uint32_t dirty_max_px;
    uint32_t interval_min_us; // entre deux fins de trame
    uint32_t interval_max_us;
    uint32_t first_refr_us; // première trame après le changement de page
    uint32_t first_render_us;
    uint32_t over_40ms; // trames dont l'intervalle dépasse 40 ms
    uint32_t over_80ms;
} stats_t;

static dashboard_navigator_t *s_navigator;
static stats_t s_stats;
static int64_t s_refr_start_us;
static int64_t s_flush_start_us;
static int64_t s_last_frame_end_us;
static int64_t s_window_start_us;
static uint64_t s_frame_flush_us;
static uint32_t s_frame_dirty_px;
static bool s_frame_rendered;
static uint32_t s_cycle;

static TaskStatus_t s_tasks_prev[MAX_TASKS];
static UBaseType_t s_tasks_prev_count;
static configRUN_TIME_COUNTER_TYPE s_total_prev;

static void display_event(lv_event_t *event) {
    const int64_t now = esp_timer_get_time();
    switch (lv_event_get_code(event)) {
    case LV_EVENT_REFR_START:
        s_refr_start_us = now;
        s_frame_flush_us = 0;
        s_frame_dirty_px = 0;
        s_frame_rendered = false;
        break;
    case LV_EVENT_RENDER_START:
        s_frame_rendered = true;
        break;
    case LV_EVENT_FLUSH_START: {
        const lv_area_t *area = lv_event_get_param(event);
        if (area != NULL) s_frame_dirty_px += lv_area_get_size(area);
        s_flush_start_us = now;
        break;
    }
    case LV_EVENT_FLUSH_FINISH:
        s_frame_flush_us += (uint64_t)(now - s_flush_start_us);
        break;
    case LV_EVENT_REFR_READY: {
        if (!s_frame_rendered) break;
        const uint32_t refr = (uint32_t)(now - s_refr_start_us);
        const uint32_t render = refr - (uint32_t)s_frame_flush_us;
        if (s_stats.frames == 0) {
            // La première trame (redessin complet) est comptée à part.
            s_stats.first_refr_us = refr;
            s_stats.first_render_us = render;
        } else {
            const uint32_t interval = (uint32_t)(now - s_last_frame_end_us);
            if (s_stats.interval_min_us == 0 ||
                interval < s_stats.interval_min_us)
                s_stats.interval_min_us = interval;
            if (interval > s_stats.interval_max_us)
                s_stats.interval_max_us = interval;
            if (interval > 40000) s_stats.over_40ms++;
            if (interval > 80000) s_stats.over_80ms++;
            s_stats.refr_us += refr;
            s_stats.flush_us += s_frame_flush_us;
            s_stats.dirty_px += s_frame_dirty_px;
            if (refr > s_stats.refr_max_us) s_stats.refr_max_us = refr;
            if (render > s_stats.render_max_us) s_stats.render_max_us = render;
            if (s_frame_dirty_px > s_stats.dirty_max_px)
                s_stats.dirty_max_px = s_frame_dirty_px;
        }
        s_stats.frames++;
        s_last_frame_end_us = now;
        break;
    }
    default:
        break;
    }
}

static void log_cpu(void) {
    static TaskStatus_t tasks[MAX_TASKS];
    configRUN_TIME_COUNTER_TYPE total = 0;
    const UBaseType_t count = uxTaskGetSystemState(tasks, MAX_TASKS, &total);
    const uint64_t elapsed = (uint64_t)(total - s_total_prev);
    if (s_tasks_prev_count > 0 && elapsed > 0) {
        for (UBaseType_t i = 0; i < count; i++) {
            for (UBaseType_t j = 0; j < s_tasks_prev_count; j++) {
                if (tasks[i].xHandle != s_tasks_prev[j].xHandle) continue;
                const uint64_t delta =
                    (uint64_t)(tasks[i].ulRunTimeCounter -
                               s_tasks_prev[j].ulRunTimeCounter);
                const uint32_t permille = (uint32_t)(delta * 1000 / elapsed);
                if (permille >= 3) {
                    ESP_LOGI(TAG, "  cpu task=%s core=%d %u.%u%% stack_hw=%u",
                             tasks[i].pcTaskName,
                             (int)xTaskGetCoreID(tasks[i].xHandle),
                             (unsigned)(permille / 10),
                             (unsigned)(permille % 10),
                             (unsigned)tasks[i].usStackHighWaterMark);
                }
                break;
            }
        }
    }
    memcpy(s_tasks_prev, tasks, sizeof(TaskStatus_t) * count);
    s_tasks_prev_count = count;
    s_total_prev = total;
}

static void bench_tick(lv_timer_t *timer) {
    (void)timer;
    const int64_t now = esp_timer_get_time();
    uint32_t window_ms = (uint32_t)((now - s_window_start_us) / 1000);
    if (window_ms == 0) window_ms = 1;
    const uint32_t n = s_stats.frames > 1 ? s_stats.frames - 1 : 1;
    const uint32_t refr_avg = (uint32_t)(s_stats.refr_us / n);
    const uint32_t flush_avg = (uint32_t)(s_stats.flush_us / n);
    ESP_LOGI(TAG,
             "cycle=%u page=%s window_ms=%u frames=%u fps=%u.%u "
             "refr_us(avg/max)=%u/%u render_us(avg/max)=%u/%u flush_us(avg)=%u "
             "dirty_px(avg/max)=%u/%u interval_us(min/max)=%u/%u "
             "late(>40ms/>80ms)=%u/%u first_frame_us(refr/render)=%u/%u",
             (unsigned)s_cycle, dashboard_navigator_current_name(s_navigator),
             (unsigned)window_ms, (unsigned)s_stats.frames,
             (unsigned)(s_stats.frames * 1000 / window_ms),
             (unsigned)(s_stats.frames * 10000 / window_ms % 10),
             (unsigned)refr_avg, (unsigned)s_stats.refr_max_us,
             (unsigned)(refr_avg - flush_avg), (unsigned)s_stats.render_max_us,
             (unsigned)flush_avg, (unsigned)(s_stats.dirty_px / n),
             (unsigned)s_stats.dirty_max_px, (unsigned)s_stats.interval_min_us,
             (unsigned)s_stats.interval_max_us, (unsigned)s_stats.over_40ms,
             (unsigned)s_stats.over_80ms, (unsigned)s_stats.first_refr_us,
             (unsigned)s_stats.first_render_us);
    log_cpu();
    ESP_LOGI(TAG, "  heap_int free/min/largest=%u/%u/%u psram free/min=%u/%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));

    if (dashboard_navigator_current(s_navigator) + 1 ==
        dashboard_navigator_count(s_navigator))
        s_cycle++;
    dashboard_navigator_next(s_navigator);
    memset(&s_stats, 0, sizeof(s_stats));
    s_window_start_us = esp_timer_get_time();
}

void perf_bench_start(lv_display_t *display, dashboard_navigator_t *navigator) {
    s_navigator = navigator;
    s_window_start_us = esp_timer_get_time();
    lv_display_add_event_cb(display, display_event, LV_EVENT_REFR_START, NULL);
    lv_display_add_event_cb(display, display_event, LV_EVENT_RENDER_START,
                            NULL);
    lv_display_add_event_cb(display, display_event, LV_EVENT_FLUSH_START, NULL);
    lv_display_add_event_cb(display, display_event, LV_EVENT_FLUSH_FINISH,
                            NULL);
    lv_display_add_event_cb(display, display_event, LV_EVENT_REFR_READY, NULL);
    lv_timer_create(bench_tick, MGF_PERF_BENCH_PAGE_MS, NULL);
    ESP_LOGI(TAG, "bench started: %u ms per page", MGF_PERF_BENCH_PAGE_MS);
}

#endif
