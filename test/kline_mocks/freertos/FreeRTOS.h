#pragma once

typedef int BaseType_t;
typedef unsigned int TickType_t;

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTRUE            1
#define pdFALSE           0
#define portMAX_DELAY     ((TickType_t) - 1)
