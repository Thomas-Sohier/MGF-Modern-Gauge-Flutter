# Firmware ESP32-S3 : écrans ambre pour le panneau rond LILYGO T-RGB.
# Pass -DMGF_LILYGO_T_RGB_V2=1 to idf.py/cmake for the vendor V2 profile.
# The default remains the original H597 profile.

# K-line is opt-in: the fake ECU remains the default firmware source. These
# cache variables become preprocessor definitions used by app_main.c.
set(MGF_USE_MEMS_KLINE 0 CACHE STRING "Use the real MEMS ECU over K-line (0/1)")
set(MGF_KLINE_UART_NUM 1 CACHE STRING "ESP32-S3 UART number for K-line")
set(MGF_KLINE_TX_GPIO 40 CACHE STRING "LILYGO T-RGB K-line TX GPIO")
set(MGF_KLINE_RX_GPIO 38 CACHE STRING "LILYGO T-RGB K-line RX GPIO")
set(MGF_KLINE_LOCAL_ECHO 0 CACHE STRING "Discard local K-line loopback echo (0/1)")
set(MGF_MEMS_VARIANT MEMS_VARIANT_1_9 CACHE STRING "MEMS variant (MEMS_VARIANT_1_9 for MGF 1995-1999, or MEMS_VARIANT_1_6)")
set(MGF_ENABLE_BLE_CONFIG 1 CACHE STRING "Enable the NimBLE link: settings + companion app services (0/1)")
set(MGF_BLE_CONFIG_OPEN_ON_BOOT 0 CACHE STRING "Open BLE configuration advertising at boot (0/1; service images only)")
set(MGF_RUNTIME_DIAGNOSTICS 0 CACHE STRING "Enable periodic runtime diagnostics (0/1)")
set(MGF_BLE_CONFIG_SRCS)
if(MGF_ENABLE_BLE_CONFIG)
    # Les CONFIG_* ne sont pas chargés pendant l'expansion des dépendances.
    if(NOT CMAKE_BUILD_EARLY_EXPANSION AND NOT CONFIG_BT_NIMBLE_ENABLED)
        message(FATAL_ERROR
            "MGF_ENABLE_BLE_CONFIG=1 requires NimBLE (CONFIG_BT_NIMBLE_ENABLED, "
            "set by sdkconfig.defaults). A stale sdkconfig generated before BLE "
            "was enabled must be deleted (see docs/ble-config.md)")
    endif()
    list(APPEND MGF_BLE_CONFIG_SRCS "infrastructure/ble_config_service.c"
                                    "infrastructure/companion_gatt.c")
endif()

# GPIO38/GPIO40 replace microSD DAT0/CMD. Do not add an SDMMC owner here.
set(MGF_LILYGO_SDMMC_ENABLED 0 CACHE INTERNAL "SDMMC is unavailable in the K-line profile" FORCE)

idf_component_register(
    SRCS
        "app_main.c"
        "app/dashboard_controller.c"
        "app/settings_coordinator.c"
        "app/settings_runtime.c"
        "domain/app_settings.c"
        "domain/display_brightness.c"
        "domain/ble_config_protocol.c"
        "domain/ble_window.c"
        "domain/companion_protocol.c"
        "domain/flat_json.c"
        "domain/mems_protocol.c"
        "domain/mems_reader.c"
        "domain/mems19_reader.c"
        "domain/mems_session.c"
        "domain/rtc_time.c"
        "infrastructure/board_display.c"
        "infrastructure/shared_i2c.c"
        "infrastructure/rtc_ds3231.c"
        "infrastructure/rtc_worker.c"
        "infrastructure/fake_ecu.c"
        "infrastructure/kline_uart_esp32.c"
        "infrastructure/mems_ecu.c"
        "infrastructure/runtime_diagnostics.c"
        "infrastructure/settings_store.c"
        "infrastructure/settings_persistence.c"
        ${MGF_BLE_CONFIG_SRCS}
        "ui/icons/dash_icons.c"
        "ui/navigation/dashboard_navigator.c"
        "ui/navigation/ui_instrumentation.c"
        "ui/screens/style_amber.c"
        "ui/screens/boot_screen.c"
        "ui/screens/clock_screen.c"
        "ui/screens/settings_screen.c"
        "ui/screens/music_screen.c"
        "ui/screens/navigation_screen.c"
        "ui/screens/faults_screen.c"
        "ui/screens/temps_screen.c"
        "ui/screens/injection_screen.c"
        "ui/screens/lambda_screen.c"
        "ui/screens/ignition_screen.c"
        "ui/screens/idle_screen.c"
        "ui/screens/admission_screen.c"
        "ui/themes/ui_theme.c"
        "ui/widgets/amber_value.c"
        "ui/widgets/amber_draw.c"
        "ui/widgets/amber_ui.c"
        "ui/widgets/amber_kit.c"
        "ui/fonts/ui_fonts.c"
    INCLUDE_DIRS "." "app" "domain" "infrastructure" "ui" "ui/screens" "ui/widgets" "ui/icons" "ui/fonts" "ui/themes"
    REQUIRES
        driver                        # GPIO + ESP-IDF compatibility types
        nvs_flash                     # persistent app settings and BT stack data
        # bt is always required: ESP-IDF resolves REQUIRES in an early pass
        # that sees neither -D cache variables nor sdkconfig, so it cannot be
        # conditional. With CONFIG_BT_ENABLED=n it builds next to nothing.
        bt
        esp_lcd
        esp_timer                     # monotonic snapshot / worker timestamps
        esp_lcd_st7701                 # ST7701S vendor init + RGB panel
        esp_lcd_panel_io_additions    # série 9 bits ST7701S via XL9535
        esp_io_expander_tca95xx_16bit # XL9535-compatible 16-bit expander
        esp_lcd_touch
        esp_lcd_touch_cst816s
        esp_lvgl_port
        lvgl
    EMBED_FILES "fonts/Michroma-Regular.ttf"
)

if(MGF_LILYGO_T_RGB_V2)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE MGF_LILYGO_T_RGB_V2=1)
endif()

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MGF_USE_MEMS_KLINE=${MGF_USE_MEMS_KLINE}
    MGF_KLINE_UART_NUM=${MGF_KLINE_UART_NUM}
    MGF_KLINE_TX_GPIO=${MGF_KLINE_TX_GPIO}
    MGF_KLINE_RX_GPIO=${MGF_KLINE_RX_GPIO}
    MGF_KLINE_LOCAL_ECHO=${MGF_KLINE_LOCAL_ECHO}
    MGF_MEMS_VARIANT=${MGF_MEMS_VARIANT}
    MGF_LILYGO_SDMMC_ENABLED=${MGF_LILYGO_SDMMC_ENABLED}
    MGF_ENABLE_BLE_CONFIG=${MGF_ENABLE_BLE_CONFIG}
    MGF_BLE_CONFIG_OPEN_ON_BOOT=${MGF_BLE_CONFIG_OPEN_ON_BOOT}
    MGF_RUNTIME_DIAGNOSTICS=${MGF_RUNTIME_DIAGNOSTICS}
)
