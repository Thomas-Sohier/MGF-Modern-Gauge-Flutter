#pragma once

#include <stdint.h>

#include "host/ble_uuid.h"
#include "os/os_mbuf.h"

#define BLE_GATT_ACCESS_OP_WRITE_CHR 1
#define BLE_GATT_SVC_TYPE_PRIMARY    1
#define BLE_GATT_CHR_F_WRITE         1
#define BLE_GATT_CHR_F_WRITE_ENC     2
#define BLE_GATT_CHR_F_WRITE_NO_RSP  4

struct ble_gatt_access_ctxt {
    uint8_t op;
    struct os_mbuf *om;
};

struct ble_gatt_chr_def {
    const ble_uuid_t *uuid;
    int (*access_cb)(uint16_t, uint16_t, struct ble_gatt_access_ctxt *, void *);
    void *arg;
    uint16_t flags;
};

struct ble_gatt_svc_def {
    uint8_t type;
    const ble_uuid_t *uuid;
    const struct ble_gatt_chr_def *characteristics;
};
