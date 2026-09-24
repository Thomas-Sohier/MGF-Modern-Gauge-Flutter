#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "domain/value_smoothing.h"

static void check(bool cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        exit(1);
    }
}

int main(void) {
    check(value_smoothing_step(NAN, 3000.0f, 40, 100.0f, 1.0f) == 3000.0f,
          "first sample snaps to target");
    check(isnan(value_smoothing_step(800.0f, NAN, 40, 100.0f, 1.0f)),
          "unavailable target stays unavailable");
    check(value_smoothing_step(800.0f, 3000.0f, 40, 0.0f, 1.0f) == 3000.0f,
          "zero time constant disables smoothing");

    const float one = value_smoothing_step(800.0f, 3000.0f, 40, 100.0f, 1.0f);
    check(one > 800.0f && one < 3000.0f, "one step moves part of the way");
    check(fabsf(value_smoothing_step(800.0f, 3000.0f, 100, 100.0f, 1.0f) -
                (800.0f + 2200.0f * (1.0f - expf(-1.0f)))) < 0.5f,
          "tau reaches 63 percent");

    float v = 800.0f;
    for (int i = 0; i < 50; i++)
        v = value_smoothing_step(v, 3000.0f, 40, 100.0f, 1.0f);
    check(v == 3000.0f, "converges and snaps exactly to target");

    printf("value smoothing tests: OK\n");
    return 0;
}
