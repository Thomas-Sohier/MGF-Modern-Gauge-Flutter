#pragma once

#include <stdint.h>

#include "os/os_mbuf.h"

#define BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN 13
#define BLE_ATT_ERR_UNLIKELY               14
#define BLE_ATT_ERR_INSUFFICIENT_AUTHEN    5

int ble_hs_mbuf_to_flat(const struct os_mbuf *om, void *out, uint16_t capacity,
                        uint16_t *copied);
