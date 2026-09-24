#include <assert.h>
#include <stddef.h>

#include "infrastructure/runtime_diagnostics.h"

int main(void) {
    runtime_diagnostics_config_t config = {
        .period_ms = 1000,
        .display_state = RUNTIME_DIAGNOSTICS_STATE_READY,
        .lvgl_state = RUNTIME_DIAGNOSTICS_STATE_READY,
        .ecu_state = RUNTIME_DIAGNOSTICS_STATE_DEGRADED,
        .ble_state = RUNTIME_DIAGNOSTICS_STATE_DISABLED,
    };
    runtime_diagnostics_t *diagnostics = runtime_diagnostics_start(&config);

#if MGF_RUNTIME_DIAGNOSTICS
#error "This host test must exercise the no-op diagnostics build"
#endif

    assert(diagnostics == NULL);
    assert(runtime_diagnostics_state_name(RUNTIME_DIAGNOSTICS_STATE_DEGRADED) !=
           NULL);
    assert(runtime_diagnostics_state_name(
               RUNTIME_DIAGNOSTICS_STATE_DEGRADED)[0] == 'd');
    runtime_diagnostics_set_service_state(diagnostics,
                                          RUNTIME_DIAGNOSTICS_SERVICE_ECU,
                                          RUNTIME_DIAGNOSTICS_STATE_READY);
    runtime_diagnostics_set_rgb_counters_provider(diagnostics, NULL, NULL);
    runtime_diagnostics_stop(diagnostics);
    return 0;
}
