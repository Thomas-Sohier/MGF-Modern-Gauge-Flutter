#pragma once

#include <stdint.h>

struct ble_gap_conn_desc {
    struct {
        uint8_t encrypted;
        uint8_t bonded;
    } sec_state;
};

int ble_gap_conn_find(uint16_t handle, struct ble_gap_conn_desc *desc);
