#include "infrastructure/ble_config_service.h"

#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "ble_config";
static bool s_started;
static ble_config_service_config_t s_config;
static char s_device_name[32];
static uint8_t s_own_addr_type;

// UUIDs are intentionally private to this service; the wire protocol version
// is carried in every value so clients can reject incompatible payloads.
static const ble_uuid128_t s_service_uuid =
    BLE_UUID128_INIT(0x9a, 0x54, 0x72, 0x31, 0x91, 0x6a, 0x4d, 0x2c,
                     0x8e, 0x44, 0x70, 0x18, 0x43, 0xb0, 0x11, 0x01);
static const ble_uuid128_t s_settings_uuid =
    BLE_UUID128_INIT(0x9a, 0x54, 0x72, 0x31, 0x91, 0x6a, 0x4d, 0x2c,
                     0x8e, 0x44, 0x70, 0x18, 0x43, 0xb0, 0x11, 0x02);
static const ble_uuid128_t s_datetime_uuid =
    BLE_UUID128_INIT(0x9a, 0x54, 0x72, 0x31, 0x91, 0x6a, 0x4d, 0x2c,
                     0x8e, 0x44, 0x70, 0x18, 0x43, 0xb0, 0x11, 0x03);

// Implemented by the ESP-IDF NimBLE port; it wires bond key persistence to
// the Bluetooth NVS namespace rather than this application's settings store.
void ble_store_config_init(void);

static bool connection_is_bonded_and_encrypted(uint16_t conn_handle) {
    struct ble_gap_conn_desc desc;
    return ble_gap_conn_find(conn_handle, &desc) == 0 &&
           desc.sec_state.encrypted && desc.sec_state.bonded;
}

static int append_settings(struct os_mbuf *om) {
    app_settings_t settings;
    uint8_t payload[BLE_CONFIG_SETTINGS_PAYLOAD_SIZE];
    size_t payload_size;
    if (s_config.settings_read == NULL ||
        !s_config.settings_read(s_config.settings_context, &settings) ||
        !ble_config_settings_encode(&settings, payload, sizeof(payload),
                                    &payload_size)) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    return os_mbuf_append(om, payload, payload_size) == 0
               ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int write_settings(uint16_t conn_handle,
                          struct ble_gatt_access_ctxt *ctxt) {
    uint8_t payload[BLE_CONFIG_SETTINGS_PAYLOAD_SIZE];
    app_settings_t settings;
    uint16_t copied_len;
    if (!connection_is_bonded_and_encrypted(conn_handle)) {
        return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    }
    if (OS_MBUF_PKTLEN(ctxt->om) != sizeof(payload)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (ble_hs_mbuf_to_flat(ctxt->om, payload, sizeof(payload), &copied_len) != 0 ||
        copied_len != sizeof(payload)) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    const ble_config_parse_result_t result =
        ble_config_settings_decode(payload, sizeof(payload), &settings);
    if (result != BLE_CONFIG_PARSE_OK) {
        ESP_LOGW(TAG, "rejected settings payload: %s",
                 ble_config_parse_result_name(result));
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (s_config.settings_update == NULL ||
        !s_config.settings_update(s_config.settings_context, &settings)) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    return 0;
}

static int write_datetime(uint16_t conn_handle,
                          struct ble_gatt_access_ctxt *ctxt) {
    uint8_t payload[BLE_CONFIG_DATETIME_PAYLOAD_SIZE];
    ble_config_datetime_t datetime;
    uint16_t copied_len;
    if (!connection_is_bonded_and_encrypted(conn_handle)) {
        return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    }
    if (OS_MBUF_PKTLEN(ctxt->om) != sizeof(payload)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (ble_hs_mbuf_to_flat(ctxt->om, payload, sizeof(payload), &copied_len) != 0 ||
        copied_len != sizeof(payload)) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    const ble_config_parse_result_t result =
        ble_config_datetime_decode(payload, sizeof(payload), &datetime);
    if (result != BLE_CONFIG_PARSE_OK) {
        ESP_LOGW(TAG, "rejected datetime payload: %s",
                 ble_config_parse_result_name(result));
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (s_config.datetime_set == NULL ||
        s_config.datetime_set(s_config.rtc_context, &datetime.date_time,
                              datetime.basis) != RTC_OK) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    return 0;
}

static int gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)attr_handle;
    (void)arg;

    if (ble_uuid_cmp(ctxt->chr->uuid, &s_settings_uuid.u) == 0) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            return append_settings(ctxt->om);
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            return write_settings(conn_handle, ctxt);
        default:
            return BLE_ATT_ERR_UNLIKELY;
        }
    }
    if (ble_uuid_cmp(ctxt->chr->uuid, &s_datetime_uuid.u) == 0) {
        return ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR
                   ? write_datetime(conn_handle, ctxt) : BLE_ATT_ERR_UNLIKELY;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

static int gap_event(struct ble_gap_event *event, void *arg);

static const struct ble_gatt_svc_def s_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &s_settings_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                         BLE_GATT_CHR_F_WRITE_ENC,
            },
            {
                .uuid = &s_datetime_uuid.u,
                .access_cb = gatt_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
            },
            { 0 },
        },
    },
    { 0 },
};

static void advertise(void) {
    struct ble_hs_adv_fields fields = {0};
    struct ble_gap_adv_params params = {0};
    int rc;

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)s_device_name;
    fields.name_len = (uint8_t)strlen(s_device_name);
    fields.name_is_complete = 1;
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "cannot set advertising fields: %d", rc);
        return;
    }

    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &params,
                           gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "cannot start advertising: %d", rc);
    }
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            const int rc = ble_gap_security_initiate(event->connect.conn_handle);
            if (rc != 0) {
                ESP_LOGW(TAG, "pairing initiation failed: %d", rc);
            }
        } else {
            advertise();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        advertise();
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        advertise();
        return 0;
    default:
        return 0;
    }
}

static void on_reset(int reason) {
    ESP_LOGE(TAG, "NimBLE reset: %d", reason);
}

static void on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "cannot infer BLE address type: %d", rc);
        return;
    }
    advertise();
}

static void host_task(void *param) {
    (void)param;
    nimble_port_run();
    vTaskDelete(NULL);
}

esp_err_t ble_config_service_start(const ble_config_service_config_t *config) {
    if (config == NULL || config->settings_read == NULL ||
        config->settings_update == NULL || config->datetime_set == NULL ||
        config->device_name == NULL || s_started) {
        return ESP_ERR_INVALID_ARG;
    }

    s_config = *config;
    strncpy(s_device_name, config->device_name, sizeof(s_device_name) - 1U);
    s_device_name[sizeof(s_device_name) - 1U] = '\0';

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        return err;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();
    if (ble_svc_gap_device_name_set(s_device_name) != 0 ||
        ble_gatts_count_cfg(s_services) != 0 ||
        ble_gatts_add_svcs(s_services) != 0) {
        return ESP_FAIL;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    // No local display/input is available for MITM passkey confirmation.
    // Bonding and link encryption are still mandatory for every write.
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC |
                                  BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ENC |
                                    BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init();
    nimble_port_freertos_init(host_task);
    s_started = true;
    ESP_LOGI(TAG, "BLE configuration service started (bonded writes required)");
    return ESP_OK;
}
