#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "domain/ecu_data.h"

// Abstraction minimale d'une source ECU : le consommateur reçoit toujours une
// copie du dernier instantané et ne peut pas accéder à l'état interne de la
// source.
typedef bool (*ecu_source_read_fn)(void *context, ecu_data_t *out);

typedef struct {
    ecu_source_read_fn read;
    void *context;
} ecu_source_t;

static inline bool ecu_source_read(const ecu_source_t *source,
                                   ecu_data_t *out) {
    return source != NULL && source->read != NULL && out != NULL &&
           source->read(source->context, out);
}
