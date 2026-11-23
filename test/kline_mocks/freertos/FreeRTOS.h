#pragma once

#include <stdint.h>

typedef int BaseType_t;
typedef unsigned int TickType_t;

// Le mock simule un tick configurable (1 kHz par défaut) ; pdMS_TO_TICKS
// arrondit à l'inférieur comme FreeRTOS.
#ifndef configTICK_RATE_HZ
#define configTICK_RATE_HZ 1000
#endif
#define pdMS_TO_TICKS(ms)                                                      \
    ((TickType_t)(((uint64_t)(ms) * configTICK_RATE_HZ) / 1000u))
#define pdTRUE        1
#define pdFALSE       0
#define pdPASS        1
#define portMAX_DELAY ((TickType_t) - 1)
