#include <stdbool.h>
#include <stdio.h>

#include "domain/ecu_source.h"

static int read_calls;
static ecu_data_t expected;

static bool read_snapshot(void *context, ecu_data_t *out) {
    ecu_data_t *snapshot = context;
    read_calls++;
    *out = *snapshot;
    return true;
}

static bool read_failure(void *context, ecu_data_t *out) {
    (void)context;
    (void)out;
    read_calls++;
    return false;
}

static void check(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        _Exit(1);
    }
}

static void test_invalid_sources(void) {
    ecu_data_t output = {0};
    ecu_source_t valid = {.read = read_snapshot, .context = &expected};
    ecu_source_t no_reader = {.read = NULL, .context = &expected};

    check(!ecu_source_read(NULL, &output), "NULL source is rejected");
    check(!ecu_source_read(&valid, NULL), "NULL output is rejected");
    check(!ecu_source_read(&no_reader, &output), "missing reader is rejected");

    read_calls = 0;
    ecu_source_t failing = {.read = read_failure, .context = NULL};
    check(!ecu_source_read(&failing, &output), "reader failure is propagated");
    check(read_calls == 1, "reader is called exactly once");
}

static void test_snapshot_copy(void) {
    expected = (ecu_data_t){
        .connected = true,
        .rpm = 4321.5f,
        .throttle = 37.25f,
        .coolant_temp = 91.0f,
        .battery_voltage = 14.15f,
        .oil_temp = 103.5f,
    };
    ecu_data_t output = {0};
    ecu_source_t source = {.read = read_snapshot, .context = &expected};

    read_calls = 0;
    check(ecu_source_read(&source, &output), "valid source succeeds");
    check(read_calls == 1, "valid reader is called once");
    check(output.connected == expected.connected, "connected field is copied");
    check(output.rpm == expected.rpm, "rpm field is copied");
    check(output.throttle == expected.throttle, "throttle field is copied");
    check(output.coolant_temp == expected.coolant_temp,
          "coolant field is copied");
    check(output.battery_voltage == expected.battery_voltage,
          "battery field is copied");
    check(output.oil_temp == expected.oil_temp, "oil field is copied");
}

int main(void) {
    test_invalid_sources();
    test_snapshot_copy();
    puts("domain tests: OK");
    return 0;
}
