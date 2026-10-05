#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "infrastructure/companion_gatt.h"

static void *allocations[16];
static size_t live_allocations;
static bool bonded = true;
static bool fail_alloc;

void *heap_caps_malloc(size_t size, unsigned caps) {
    (void)caps;
    if (fail_alloc) return NULL;
    void *p = malloc(size);
    assert(p != NULL);
    assert(live_allocations < 16);
    allocations[live_allocations++] = p;
    return p;
}

void heap_caps_free(void *p) {
    size_t i = 0;
    while (i < live_allocations && allocations[i] != p) i++;
    assert(i <
           live_allocations); // Also catches double-free / non-owned buffers.
    allocations[i] = allocations[--live_allocations];
    free(p);
}

int ble_gap_conn_find(uint16_t handle, struct ble_gap_conn_desc *desc) {
    assert(handle == 1);
    desc->sec_state.encrypted = true;
    desc->sec_state.bonded = bonded;
    return 0;
}

int ble_hs_mbuf_to_flat(const struct os_mbuf *om, void *out, uint16_t capacity,
                        uint16_t *copied) {
    if (om->length > capacity) return -1;
    memcpy(out, om->data, om->length);
    *copied = om->length;
    return 0;
}

// Exercise the actual registered callbacks, not duplicated control logic.
static int write_chr(uint8_t uuid_suffix, const void *data, uint16_t length) {
    const struct ble_gatt_chr_def *chr =
        companion_gatt_services()[0].characteristics;
    for (; chr->uuid != NULL; chr++) {
        const ble_uuid128_t *uuid = (const ble_uuid128_t *)chr->uuid;
        if (uuid->value[12] != uuid_suffix) continue;
        struct os_mbuf om = {.data = data, .length = length};
        struct ble_gatt_access_ctxt ctxt = {.op = BLE_GATT_ACCESS_OP_WRITE_CHR,
                                            .om = &om};
        return chr->access_cb(1, 0, &ctxt, chr->arg);
    }
    assert(false);
    return -1;
}

static int control(void) {
    const char json[] =
        "{\"art_id\":\"cover\",\"total_bytes\":7,\"chunk_count\":2}";
    return write_chr(3, json, sizeof(json) - 1);
}

static const uint8_t first[] = {0, 0, 0xff, 0xd8, 0, 0x42};
static const uint8_t last[] = {0, 1, 0x80, 0xff, 0xd9};
static const uint8_t expected[] = {0xff, 0xd8, 0, 0x42, 0x80, 0xff, 0xd9};

static void complete(void) {
    assert(write_chr(4, first, sizeof(first)) == 0);
    assert(write_chr(4, last, sizeof(last)) == 0);
}

int main(void) {
    companion_art_jpeg_t jpeg = {0};
    companion_gatt_reset();
    assert(!companion_gatt_take_art(&jpeg));
    bonded = false;
    assert(control() == BLE_ATT_ERR_INSUFFICIENT_AUTHEN);
    assert(live_allocations == 0);
    bonded = true;

    assert(control() == 0);
    assert(live_allocations == 1);
    assert(!companion_gatt_take_art(&jpeg));
    assert(write_chr(4, first, sizeof(first)) == 0);
    assert(!companion_gatt_take_art(&jpeg));
    assert(write_chr(4, last, sizeof(last)) == 0);
    assert(!companion_gatt_take_art(NULL));
    assert(companion_gatt_take_art(
        &jpeg)); // Fails without s_art_buffer assignment.
    assert(jpeg.data == allocations[0]);
    assert(jpeg.length == sizeof(expected));
    assert(memcmp(jpeg.data, expected, sizeof(expected)) == 0);
    assert(!companion_gatt_take_art(&jpeg));
    companion_gatt_reset(); // Ownership has moved to the caller, not reset.
    assert(live_allocations == 1);
    assert(memcmp(jpeg.data, expected, sizeof(expected)) == 0);
    companion_gatt_free_art(jpeg.data);
    assert(live_allocations == 0);

    assert(control() == 0);
    assert(control() == 0); // Replace an incomplete transfer.
    assert(live_allocations == 1);
    complete();
    assert(control() == 0); // Replace a completed but unconsumed transfer.
    assert(live_allocations == 1);
    assert(!companion_gatt_take_art(&jpeg));
    complete();
    companion_gatt_reset();
    assert(live_allocations == 0);
    assert(!companion_gatt_take_art(&jpeg));

    assert(control() == 0);
    assert(write_chr(4, last, sizeof(last)) == BLE_ATT_ERR_UNLIKELY);
    assert(live_allocations == 0); // Invalid chunk aborts and frees ownership.
    assert(!companion_gatt_take_art(&jpeg));

    assert(control() == 0);
    complete();
    fail_alloc = true;
    assert(control() == BLE_ATT_ERR_UNLIKELY);
    assert(live_allocations == 0);
    assert(!companion_gatt_take_art(&jpeg));
    fail_alloc = false;
    assert(control() == 0);
    complete();
    assert(companion_gatt_take_art(&jpeg));
    companion_gatt_free_art(jpeg.data);
    companion_gatt_reset();
    assert(live_allocations == 0);
    puts("companion GATT ownership tests passed");
    return 0;
}
