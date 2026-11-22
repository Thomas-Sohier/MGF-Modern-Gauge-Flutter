#pragma once

#include "lvgl.h"
#include "ui/navigation/dashboard_navigator.h"

#ifndef MGF_PERF_BENCH
#define MGF_PERF_BENCH 0
#endif

#if MGF_PERF_BENCH && defined(ESP_PLATFORM)
// Banc de mesure : fait défiler les pages et journalise, pour chacune, les
// temps de rendu/flush LVGL et la charge CPU par tâche. À appeler sous le
// verrou LVGL.
void perf_bench_start(lv_display_t *display, dashboard_navigator_t *navigator);
#else
static inline void perf_bench_start(lv_display_t *display,
                                    dashboard_navigator_t *navigator) {
    (void)display;
    (void)navigator;
}
#endif
