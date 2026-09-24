#include "infrastructure/runtime_diagnostics.h"

#if MGF_RUNTIME_DIAGNOSTICS && defined(ESP_PLATFORM)

#include <stddef.h>
#include <stdlib.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define DEFAULT_PERIOD_MS         5000U
#define DIAGNOSTICS_TASK_STACK    4096U
#define DIAGNOSTICS_TASK_PRIORITY (tskIDLE_PRIORITY + 1U)
#define MAX_TASKS_SNAPSHOT        32U

static const char *TAG = "mgf_diag";
static portMUX_TYPE s_state_lock = portMUX_INITIALIZER_UNLOCKED;
static runtime_diagnostics_t *s_instance;

struct runtime_diagnostics_s {
    TaskHandle_t task;
    SemaphoreHandle_t stopped;
    volatile bool running;
    uint32_t period_ms;
    runtime_diagnostics_state_t
        service_states[RUNTIME_DIAGNOSTICS_SERVICE_COUNT];
    runtime_diagnostics_rgb_counters_cb_t rgb_provider;
    void *rgb_context;
};

#if defined(configUSE_TRACE_FACILITY) && configUSE_TRACE_FACILITY
static char task_state_name(eTaskState state) {
    switch (state) {
    case eRunning:
        return 'R';
    case eReady:
        return 'r';
    case eBlocked:
        return 'B';
    case eSuspended:
        return 'S';
    case eDeleted:
        return 'D';
    default:
        return '?';
    }
}
#endif

static void log_task_snapshot(void) {
#if defined(configUSE_TRACE_FACILITY) && configUSE_TRACE_FACILITY
    // This is deliberately static: uxTaskGetSystemState() must not allocate
    // in a periodic diagnostic path, and the array is large enough for this
    // firmware's normal task set. A truncated snapshot is reported explicitly.
    static TaskStatus_t task_snapshot[MAX_TASKS_SNAPSHOT];

    const UBaseType_t task_count = uxTaskGetNumberOfTasks();
    const UBaseType_t requested =
        task_count > MAX_TASKS_SNAPSHOT ? MAX_TASKS_SNAPSHOT : task_count;
    configRUN_TIME_COUNTER_TYPE total_runtime = 0;
    const UBaseType_t returned =
        uxTaskGetSystemState(task_snapshot, requested, &total_runtime);

    ESP_LOGI(TAG, "tasks=%u%s", (unsigned)returned,
             task_count > MAX_TASKS_SNAPSHOT ? " (snapshot truncated)" : "");
    for (UBaseType_t i = 0; i < returned; ++i) {
        const TaskStatus_t *task = &task_snapshot[i];
        ESP_LOGI(TAG, "task=%s state=%c prio=%u stack_hw=%u",
                 task->pcTaskName != NULL ? task->pcTaskName : "?",
                 task_state_name(task->eCurrentState),
                 (unsigned)task->uxCurrentPriority,
                 (unsigned)task->usStackHighWaterMark);
    }
#else
    ESP_LOGW(
        TAG,
        "task stack high-water marks unavailable: FreeRTOS trace facility disabled");
#endif
}

static void log_snapshot(runtime_diagnostics_t *diagnostics) {
    runtime_diagnostics_state_t states[RUNTIME_DIAGNOSTICS_SERVICE_COUNT];
    runtime_diagnostics_rgb_counters_cb_t rgb_provider;
    void *rgb_context;

    portENTER_CRITICAL(&s_state_lock);
    for (size_t i = 0; i < RUNTIME_DIAGNOSTICS_SERVICE_COUNT; ++i) {
        states[i] = diagnostics->service_states[i];
    }
    rgb_provider = diagnostics->rgb_provider;
    rgb_context = diagnostics->rgb_context;
    portEXIT_CRITICAL(&s_state_lock);

    ESP_LOGI(
        TAG,
        "services display=%s lvgl=%s ecu=%s ble=%s "
        "heap_int free/min/largest=%u/%u/%u "
        "psram free/min/largest=%u/%u/%u",
        runtime_diagnostics_state_name(
            states[RUNTIME_DIAGNOSTICS_SERVICE_DISPLAY]),
        runtime_diagnostics_state_name(
            states[RUNTIME_DIAGNOSTICS_SERVICE_LVGL]),
        runtime_diagnostics_state_name(states[RUNTIME_DIAGNOSTICS_SERVICE_ECU]),
        runtime_diagnostics_state_name(states[RUNTIME_DIAGNOSTICS_SERVICE_BLE]),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
        (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

    runtime_diagnostics_rgb_counters_t counters = {0};
    if (rgb_provider != NULL && rgb_provider(rgb_context, &counters) &&
        counters.available) {
        ESP_LOGI(TAG, "rgb underrun=%u frame_errors=%u",
                 (unsigned)counters.underrun_count,
                 (unsigned)counters.frame_error_count);
    } else {
        ESP_LOGI(TAG, "rgb counters=unavailable "
                      "(esp_lcd RGB API exposes no underrun counter)");
    }
    log_task_snapshot();
}

static void diagnostics_task(void *arg) {
    runtime_diagnostics_t *diagnostics = arg;

    while (diagnostics->running) {
        log_snapshot(diagnostics);
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(diagnostics->period_ms));
    }

    xSemaphoreGive(diagnostics->stopped);
    vTaskDelete(NULL);
}

runtime_diagnostics_t *
runtime_diagnostics_start(const runtime_diagnostics_config_t *config) {
    if (config == NULL || s_instance != NULL) return NULL;

    runtime_diagnostics_t *diagnostics = calloc(1, sizeof(*diagnostics));
    if (diagnostics == NULL) return NULL;

    diagnostics->period_ms =
        config->period_ms > 0 ? config->period_ms : DEFAULT_PERIOD_MS;
    diagnostics->service_states[RUNTIME_DIAGNOSTICS_SERVICE_DISPLAY] =
        config->display_state;
    diagnostics->service_states[RUNTIME_DIAGNOSTICS_SERVICE_LVGL] =
        config->lvgl_state;
    diagnostics->service_states[RUNTIME_DIAGNOSTICS_SERVICE_ECU] =
        config->ecu_state;
    diagnostics->service_states[RUNTIME_DIAGNOSTICS_SERVICE_BLE] =
        config->ble_state;
    diagnostics->stopped = xSemaphoreCreateBinary();
    if (diagnostics->stopped == NULL) {
        free(diagnostics);
        return NULL;
    }

    diagnostics->running = true;
    s_instance = diagnostics;
    if (xTaskCreate(diagnostics_task, "mgf_diag", DIAGNOSTICS_TASK_STACK,
                    diagnostics, DIAGNOSTICS_TASK_PRIORITY,
                    &diagnostics->task) != pdPASS) {
        s_instance = NULL;
        vSemaphoreDelete(diagnostics->stopped);
        free(diagnostics);
        return NULL;
    }
    return diagnostics;
}

void runtime_diagnostics_stop(runtime_diagnostics_t *diagnostics) {
    if (diagnostics == NULL) return;

    diagnostics->running = false;
    if (diagnostics->task != NULL) {
        xTaskNotifyGive(diagnostics->task);
        (void)xSemaphoreTake(diagnostics->stopped, portMAX_DELAY);
    }
    if (s_instance == diagnostics) s_instance = NULL;
    vSemaphoreDelete(diagnostics->stopped);
    free(diagnostics);
}

void runtime_diagnostics_set_service_state(
    runtime_diagnostics_t *diagnostics, runtime_diagnostics_service_t service,
    runtime_diagnostics_state_t state) {
    if (diagnostics == NULL || service >= RUNTIME_DIAGNOSTICS_SERVICE_COUNT) {
        return;
    }
    portENTER_CRITICAL(&s_state_lock);
    diagnostics->service_states[service] = state;
    portEXIT_CRITICAL(&s_state_lock);
}

void runtime_diagnostics_set_rgb_counters_provider(
    runtime_diagnostics_t *diagnostics,
    runtime_diagnostics_rgb_counters_cb_t provider, void *context) {
    if (diagnostics == NULL) return;
    portENTER_CRITICAL(&s_state_lock);
    diagnostics->rgb_provider = provider;
    diagnostics->rgb_context = context;
    portEXIT_CRITICAL(&s_state_lock);
}

#endif
