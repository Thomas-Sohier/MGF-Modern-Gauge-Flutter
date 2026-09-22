# Persistent application settings

Application preferences are stored in the ESP-IDF `nvs` partition through
`main/infrastructure/settings_store.[ch]`. The small typed model currently
contains brightness (0–100%), selected dashboard page, theme, and units. The
current defaults are 100% brightness, RPM page, amber theme, and metric units.

The application schema is versioned (`APP_SETTINGS_SCHEMA_VERSION`). A missing
namespace, unsupported schema, incomplete record, NVS type mismatch, or invalid
enum/range is treated as a clean first boot and loaded from defaults. Loading
does not rewrite flash. Future schema migrations should be added explicitly
rather than interpreting an unknown version as a current record.

The intended lifecycle is:

1. Call `settings_store_init()` once during startup.
2. Call `settings_store_load()` once; it fills defaults when stored data is not
   usable.
3. Give the loaded snapshot to `app/settings_coordinator.[ch]`. The application
   restores `selected_page` after registering dashboard pages and registers the
   navigator's page-change observer.
4. Route local setting changes through `app/settings_runtime.[ch]`. The runtime
   applies them in the LVGL task and delegates persistence to the coordinator;
   the 3-second quiet period batches changes, and a failed save remains dirty
   for retry.

`settings_runtime_read()` is used by the optional BLE service. The NimBLE
service queues accepted writes in its own protected single-slot hand-off; the
LVGL timer consumes that slot through `ble_config_service_take_settings_update`,
applies brightness, units and page, then lets the coordinator save it. The
runtime's `settings_runtime_submit()` remains the equivalent public hand-off for
other transports. No callback in the NimBLE task calls LVGL or NVS. BLE writes
therefore acknowledge acceptance into RAM; the NVS commit is asynchronous and
may still be retried.

Brightness is mapped from 0–100% to the AW9364's 0–16 discrete levels. Units
currently update temperatures (°C/°F) and MAP pressure (kPa/psi) on the amber
RPM, temperatures and admission pages; RPM, voltage, percentages and timing
values are unchanged. The theme field remains validated and persisted, but the
only compiled theme is amber. There is still no dedicated on-device settings
screen; the public runtime setters are the application boundary for one.

The coordinator remains independent of LVGL and NVS. `settings_store_save()`
remains the only NVS adapter; it skips both `nvs_set_*` calls and `nvs_commit()`
when the snapshot is unchanged. It is called by the LVGL settings timer, not by
the ECU/display update loop or the NimBLE task.

## Bluetooth data

Bluetooth bonding remains owned by the ESP-IDF Bluetooth stack in the same NVS
partition. Application settings use only the `app_settings` namespace and do
not read, write, or clear Bluetooth namespaces. The store intentionally does
not automatically call `nvs_flash_erase()` when NVS initialization reports a
recovery error, because that would remove bonded devices. Recovery that needs
a full NVS erase must be an explicit field-service operation and will require
pairing devices again.

The current custom partition table allocates 24 KiB to NVS, which is shared by
these small settings and ESP-IDF-managed Bluetooth data. It has not been
validated on hardware in this environment.
