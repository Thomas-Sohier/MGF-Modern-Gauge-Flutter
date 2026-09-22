#pragma once

#include <stdint.h>

#ifndef MGF_UI_INSTRUMENTATION
#define MGF_UI_INSTRUMENTATION 0
#endif

#if MGF_UI_INSTRUMENTATION && defined(ESP_PLATFORM)
uint64_t ui_instrumentation_begin(void);
void ui_instrumentation_end(const char *page, uint64_t started_at);
#else
static inline uint64_t ui_instrumentation_begin(void) {
    return 0;
}

static inline void ui_instrumentation_end(const char *page, uint64_t started_at) {
    (void)page;
    (void)started_at;
}
#endif
