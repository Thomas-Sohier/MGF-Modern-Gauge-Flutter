#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "domain/ecu_freshness.h"

static void check(bool cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        exit(1);
    }
}

int main(void) {
    const uint64_t base = 1000000;

    check(ecu_snapshot_is_fresh(true, base, base), "age 0 is fresh");
    check(ecu_snapshot_is_fresh(true, base, base + 1499999),
          "age 1499999 is fresh");
    check(!ecu_snapshot_is_fresh(true, base, base + 1500000),
          "age 1500000 is stale");
    check(!ecu_snapshot_is_fresh(true, base, base + 1500001),
          "age 1500001 is stale");
    check(!ecu_snapshot_is_fresh(false, base, base),
          "uninitialized snapshot is not fresh");
    check(!ecu_snapshot_is_fresh(true, base, base - 1),
          "future timestamp is not fresh");

    // Des lectures monotones proches du maximum ne doivent pas déborder.
    check(!ecu_snapshot_is_fresh(true, 0, UINT64_MAX),
          "huge age is stale without overflow");
    check(ecu_snapshot_is_fresh(true, UINT64_MAX - 1, UINT64_MAX),
          "age 1 near max is fresh without overflow");
    check(ecu_snapshot_is_fresh(true, UINT64_MAX, UINT64_MAX),
          "max timestamp with age 0 is fresh");

    printf("ecu freshness tests: OK\n");
    return 0;
}
