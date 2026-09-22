#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef MGF_RUNTIME_DIAGNOSTICS
#define MGF_RUNTIME_DIAGNOSTICS 0
#endif

typedef struct runtime_diagnostics_s runtime_diagnostics_t;

typedef enum {
    RUNTIME_DIAGNOSTICS_SERVICE_DISPLAY = 0,
    RUNTIME_DIAGNOSTICS_SERVICE_LVGL,
    RUNTIME_DIAGNOSTICS_SERVICE_ECU,
    RUNTIME_DIAGNOSTICS_SERVICE_BLE,
    RUNTIME_DIAGNOSTICS_SERVICE_COUNT,
} runtime_diagnostics_service_t;

typedef enum {
    RUNTIME_DIAGNOSTICS_STATE_DISABLED = 0,
    RUNTIME_DIAGNOSTICS_STATE_STARTING,
    RUNTIME_DIAGNOSTICS_STATE_READY,
    RUNTIME_DIAGNOSTICS_STATE_DEGRADED,
    RUNTIME_DIAGNOSTICS_STATE_ERROR,
} runtime_diagnostics_state_t;

typedef struct {
    uint32_t period_ms; // 0 selects the implementation default.
    runtime_diagnostics_state_t display_state;
    runtime_diagnostics_state_t lvgl_state;
    runtime_diagnostics_state_t ecu_state;
    runtime_diagnostics_state_t ble_state;
} runtime_diagnostics_config_t;

// `available` must be checked before using the counters. The current ESP-IDF
// RGB panel API does not expose an underrun counter, so no provider is
// registered by the board code and diagnostics report it as unavailable.
typedef struct {
    bool available;
    uint32_t underrun_count;
    uint32_t frame_error_count;
} runtime_diagnostics_rgb_counters_t;

typedef bool (*runtime_diagnostics_rgb_counters_cb_t)(
    void *context, runtime_diagnostics_rgb_counters_t *out);

static inline const char *runtime_diagnostics_state_name(
    runtime_diagnostics_state_t state) {
    switch (state) {
    case RUNTIME_DIAGNOSTICS_STATE_DISABLED: return "disabled";
    case RUNTIME_DIAGNOSTICS_STATE_STARTING: return "starting";
    case RUNTIME_DIAGNOSTICS_STATE_READY: return "ready";
    case RUNTIME_DIAGNOSTICS_STATE_DEGRADED: return "degraded";
    case RUNTIME_DIAGNOSTICS_STATE_ERROR: return "error";
    default: return "unknown";
    }
}

#if MGF_RUNTIME_DIAGNOSTICS && defined(ESP_PLATFORM)

// Starts one low-priority task. It uses a fixed task-status buffer and makes
// no recurring allocations. The returned object is owned by the caller.
runtime_diagnostics_t *runtime_diagnostics_start(
    const runtime_diagnostics_config_t *config);
void runtime_diagnostics_stop(runtime_diagnostics_t *diagnostics);

// State changes are sampled by the diagnostics task and are not logged by the
// caller, which keeps startup/callback paths quiet and ISR-safe.
void runtime_diagnostics_set_service_state(
    runtime_diagnostics_t *diagnostics,
    runtime_diagnostics_service_t service,
    runtime_diagnostics_state_t state);

// Optional extension point for a future panel/SoC API that exposes hardware
// counters. The callback is called from the diagnostics task, never an ISR.
void runtime_diagnostics_set_rgb_counters_provider(
    runtime_diagnostics_t *diagnostics,
    runtime_diagnostics_rgb_counters_cb_t provider,
    void *context);

#else

static inline runtime_diagnostics_t *runtime_diagnostics_start(
    const runtime_diagnostics_config_t *config) {
    (void)config;
    return NULL;
}

static inline void runtime_diagnostics_stop(runtime_diagnostics_t *diagnostics) {
    (void)diagnostics;
}

static inline void runtime_diagnostics_set_service_state(
    runtime_diagnostics_t *diagnostics,
    runtime_diagnostics_service_t service,
    runtime_diagnostics_state_t state) {
    (void)diagnostics;
    (void)service;
    (void)state;
}

static inline void runtime_diagnostics_set_rgb_counters_provider(
    runtime_diagnostics_t *diagnostics,
    runtime_diagnostics_rgb_counters_cb_t provider,
    void *context) {
    (void)diagnostics;
    (void)provider;
    (void)context;
}

#endif
