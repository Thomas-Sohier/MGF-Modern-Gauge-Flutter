#pragma once

#include <stdbool.h>
#include <stdint.h>

// Fenêtre de maintenance BLE ouverte depuis l'écran de réglages : le service
// n'annonce que pendant une durée bornée, prolongée tant qu'un client est
// connecté. Pur C, sans horloge propre : l'appelant fournit `now_ms`.
#define BLE_WINDOW_DURATION_MS (5U * 60U * 1000U)

typedef struct {
    bool open;
    uint32_t deadline_ms;
} ble_window_t;

void ble_window_open(ble_window_t *window, uint32_t now_ms);
void ble_window_close(ble_window_t *window);
// Retourne true une seule fois, quand la fenêtre expire : l'appelant ferme
// alors le service. Une connexion active repousse l'échéance.
bool ble_window_tick(ble_window_t *window, uint32_t now_ms, bool connected);
// Secondes restantes, arrondies au supérieur ; 0 si la fenêtre est fermée.
uint32_t ble_window_remaining_s(const ble_window_t *window, uint32_t now_ms);
