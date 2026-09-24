#include "infrastructure/companion_gatt.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "os/os_mbuf.h"

static const char *TAG = "companion";

// 7f3a00NN-9c44-4e6b-8d2a-5b1f00000001, octets en ordre little-endian.
#define COMPANION_UUID(n)                                                      \
    BLE_UUID128_INIT(0x01, 0x00, 0x00, 0x00, 0x1f, 0x5b, 0x2a, 0x8d, 0x6b,     \
                     0x4e, 0x44, 0x9c, (n), 0x00, 0x3a, 0x7f)

static const ble_uuid128_t s_service_uuid = COMPANION_UUID(0x01);
static const ble_uuid128_t s_metadata_uuid = COMPANION_UUID(0x02);
static const ble_uuid128_t s_art_control_uuid = COMPANION_UUID(0x03);
static const ble_uuid128_t s_art_data_uuid = COMPANION_UUID(0x04);
static const ble_uuid128_t s_nav_uuid = COMPANION_UUID(0x05);
static const ble_uuid128_t s_nav_icon_control_uuid = COMPANION_UUID(0x06);
static const ble_uuid128_t s_nav_icon_data_uuid = COMPANION_UUID(0x07);
static const ble_uuid128_t s_alert_uuid = COMPANION_UUID(0x08);
static const ble_uuid128_t s_command_uuid = COMPANION_UUID(0x09);
// 0x0a était le catalogue retiré (ADR 0006 de l'application) : non réutilisé.
static const ble_uuid128_t s_time_uuid = COMPANION_UUID(0x0b);

#define KEY_QUEUE_LENGTH 8U

static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static companion_media_t s_media;
static bool s_media_pending;
static companion_nav_t s_nav;
static bool s_nav_pending;
static companion_time_t s_time;
static bool s_time_pending;
static companion_key_t s_keys[KEY_QUEUE_LENGTH];
static size_t s_key_head;
static size_t s_key_count;

// Les accès GATT sont sérialisés dans la tâche NimBLE : un tampon statique
// évite 512 octets de pile par écriture.
static char s_payload[COMPANION_PAYLOAD_MAX + 1U];

typedef enum {
    CHR_METADATA = 0,
    CHR_DISCARD,
    CHR_NAV,
    CHR_COMMAND,
    CHR_TIME,
} chr_kind_t;

static bool link_is_bonded(uint16_t conn_handle) {
    struct ble_gap_conn_desc desc;
    return ble_gap_conn_find(conn_handle, &desc) == 0 &&
           desc.sec_state.encrypted && desc.sec_state.bonded;
}

static int read_payload(struct os_mbuf *om, size_t *length) {
    const uint16_t total = OS_MBUF_PKTLEN(om);
    if (total == 0 || total > COMPANION_PAYLOAD_MAX) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    uint16_t copied = 0;
    if (ble_hs_mbuf_to_flat(om, s_payload, COMPANION_PAYLOAD_MAX, &copied) !=
            0 ||
        copied != total) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    s_payload[copied] = '\0';
    *length = copied;
    return 0;
}

static int handle_write(chr_kind_t kind, struct os_mbuf *om) {
    if (kind == CHR_DISCARD) return 0;
    size_t length = 0;
    const int rc = read_payload(om, &length);
    if (rc != 0) return rc;

    switch (kind) {
    case CHR_METADATA: {
        companion_media_t media;
        if (!companion_parse_media(s_payload, length, &media)) break;
        portENTER_CRITICAL(&s_lock);
        s_media = media;
        s_media_pending = true;
        portEXIT_CRITICAL(&s_lock);
        return 0;
    }
    case CHR_NAV: {
        companion_nav_t nav;
        if (!companion_parse_nav(s_payload, length, &nav)) break;
        portENTER_CRITICAL(&s_lock);
        s_nav = nav;
        s_nav_pending = true;
        portEXIT_CRITICAL(&s_lock);
        return 0;
    }
    case CHR_COMMAND: {
        companion_key_t key;
        if (!companion_parse_key(s_payload, length, &key)) break;
        portENTER_CRITICAL(&s_lock);
        if (s_key_count < KEY_QUEUE_LENGTH) {
            s_keys[(s_key_head + s_key_count) % KEY_QUEUE_LENGTH] = key;
            s_key_count++;
        }
        portEXIT_CRITICAL(&s_lock);
        return 0;
    }
    case CHR_TIME: {
        companion_time_t time;
        if (!companion_parse_time(s_payload, length, &time)) break;
        portENTER_CRITICAL(&s_lock);
        s_time = time;
        s_time_pending = true;
        portEXIT_CRITICAL(&s_lock);
        return 0;
    }
    default:
        return 0;
    }
    ESP_LOGW(TAG, "rejected payload on characteristic %d (%u bytes)", (int)kind,
             (unsigned)length);
    return BLE_ATT_ERR_UNLIKELY;
}

static int access_cb(uint16_t conn_handle, uint16_t attr_handle,
                     struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)attr_handle;
    if (ctxt == NULL || ctxt->om == NULL ||
        ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    // Chiffrement exigé par les drapeaux GATT ; le lien doit en plus provenir
    // d'un téléphone appairé (clé stockée), pas d'un appairage éphémère.
    if (!link_is_bonded(conn_handle)) return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    return handle_write((chr_kind_t)(uintptr_t)arg, ctxt->om);
}

#define WRITE_FLAGS  (BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC)
#define STREAM_FLAGS (BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE_ENC)

static const struct ble_gatt_svc_def s_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {.uuid = &s_metadata_uuid.u,
                 .access_cb = access_cb,
                 .arg = (void *)CHR_METADATA,
                 .flags = WRITE_FLAGS},
                {.uuid = &s_art_control_uuid.u,
                 .access_cb = access_cb,
                 .arg = (void *)CHR_DISCARD,
                 .flags = WRITE_FLAGS},
                {.uuid = &s_art_data_uuid.u,
                 .access_cb = access_cb,
                 .arg = (void *)CHR_DISCARD,
                 .flags = STREAM_FLAGS},
                {.uuid = &s_nav_uuid.u,
                 .access_cb = access_cb,
                 .arg = (void *)CHR_NAV,
                 .flags = WRITE_FLAGS},
                {.uuid = &s_nav_icon_control_uuid.u,
                 .access_cb = access_cb,
                 .arg = (void *)CHR_DISCARD,
                 .flags = WRITE_FLAGS},
                {.uuid = &s_nav_icon_data_uuid.u,
                 .access_cb = access_cb,
                 .arg = (void *)CHR_DISCARD,
                 .flags = STREAM_FLAGS},
                {.uuid = &s_alert_uuid.u,
                 .access_cb = access_cb,
                 .arg = (void *)CHR_DISCARD,
                 .flags = WRITE_FLAGS},
                {.uuid = &s_command_uuid.u,
                 .access_cb = access_cb,
                 .arg = (void *)CHR_COMMAND,
                 .flags = WRITE_FLAGS},
                {.uuid = &s_time_uuid.u,
                 .access_cb = access_cb,
                 .arg = (void *)CHR_TIME,
                 .flags = WRITE_FLAGS},
                {0},
            },
    },
    {0},
};

const struct ble_gatt_svc_def *companion_gatt_services(void) {
    return s_services;
}

const ble_uuid_t *companion_gatt_service_uuid(void) {
    return &s_service_uuid.u;
}

void companion_gatt_reset(void) {
    portENTER_CRITICAL(&s_lock);
    s_media_pending = false;
    s_nav_pending = false;
    s_time_pending = false;
    s_key_count = 0;
    portEXIT_CRITICAL(&s_lock);
}

bool companion_gatt_take_media(companion_media_t *out) {
    if (out == NULL) return false;
    portENTER_CRITICAL(&s_lock);
    const bool pending = s_media_pending;
    if (pending) *out = s_media;
    s_media_pending = false;
    portEXIT_CRITICAL(&s_lock);
    return pending;
}

bool companion_gatt_take_nav(companion_nav_t *out) {
    if (out == NULL) return false;
    portENTER_CRITICAL(&s_lock);
    const bool pending = s_nav_pending;
    if (pending) *out = s_nav;
    s_nav_pending = false;
    portEXIT_CRITICAL(&s_lock);
    return pending;
}

bool companion_gatt_take_time(companion_time_t *out) {
    if (out == NULL) return false;
    portENTER_CRITICAL(&s_lock);
    const bool pending = s_time_pending;
    if (pending) *out = s_time;
    s_time_pending = false;
    portEXIT_CRITICAL(&s_lock);
    return pending;
}

bool companion_gatt_take_key(companion_key_t *out) {
    if (out == NULL) return false;
    portENTER_CRITICAL(&s_lock);
    const bool pending = s_key_count > 0;
    if (pending) {
        *out = s_keys[s_key_head];
        s_key_head = (s_key_head + 1U) % KEY_QUEUE_LENGTH;
        s_key_count--;
    }
    portEXIT_CRITICAL(&s_lock);
    return pending;
}
