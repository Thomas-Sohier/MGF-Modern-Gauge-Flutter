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
4. Route setting changes through the coordinator's typed setters. A 3-second
   quiet period batches changes, and a failed save remains dirty for retry.

The coordinator is independent of LVGL and NVS, so future brightness, theme, or
units controls can use the same boundary without placing storage calls in UI
code. `settings_store_save()` remains the only NVS adapter; it skips both
`nvs_set_*` calls and `nvs_commit()` when the snapshot is unchanged. It must not
be called from the ECU/display update loop. No settings UI is included yet.

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
