#pragma once

#include "freertos/FreeRTOS.h"

typedef unsigned UBaseType_t;
typedef void (*TaskFunction_t)(void *);
typedef struct host_task *TaskHandle_t;

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack,
                       void *arg, UBaseType_t priority, TaskHandle_t *out);
void vTaskDelete(TaskHandle_t task);
TickType_t xTaskGetTickCount(void);
BaseType_t xTaskDelayUntil(TickType_t *previous, TickType_t increment);
void vTaskDelay(TickType_t ticks);
