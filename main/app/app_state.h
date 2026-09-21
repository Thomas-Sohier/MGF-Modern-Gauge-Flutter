#pragma once

// État de connexion applicatif du tableau de bord.
typedef enum {
    DASHBOARD_STATE_BOOT,
    DASHBOARD_STATE_CONNECTED,
    DASHBOARD_STATE_DISCONNECTED,
    DASHBOARD_STATE_ERROR,
} dashboard_state_t;
