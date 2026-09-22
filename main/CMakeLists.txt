# Firmware ESP32-S3 : seul l'écran ambre est embarqué (les styles dual/cream et
# leurs icônes ne servent qu'au simulateur hôte, cf. sim/).
# Pass -DMGF_LILYGO_T_RGB_V2=1 to idf.py/cmake for the vendor V2 profile.
# The default remains the original H597 profile.

# K-line is opt-in: the fake ECU remains the default firmware source. These
# cache variables become preprocessor definitions used by app_main.c.
set(MGF_USE_MEMS_KLINE 0 CACHE STRING "Use the real MEMS ECU over K-line (0/1)")
set(MGF_KLINE_UART_NUM 1 CACHE STRING "ESP32-S3 UART number for K-line")
set(MGF_KLINE_TX_GPIO 40 CACHE STRING "LILYGO T-RGB K-line TX GPIO")
set(MGF_KLINE_RX_GPIO 38 CACHE STRING "LILYGO T-RGB K-line RX GPIO")
set(MGF_KLINE_LOCAL_ECHO 0 CACHE STRING "Discard local K-line loopback echo (0/1)")
set(MGF_MEMS_VARIANT MEMS_VARIANT_1_6 CACHE STRING "MEMS variant (MEMS_VARIANT_1_6 or MEMS_VARIANT_1_9)")
set(MGF_ENABLE_BLE_CONFIG 0 CACHE STRING "Enable optional NimBLE configuration service (0/1)")
set(MGF_BLE_CONFIG_SRCS)
set(MGF_BLE_CONFIG_REQUIRES)
if(MGF_ENABLE_BLE_CONFIG)
    list(APPEND MGF_BLE_CONFIG_SRCS "infrastructure/ble_config_service.c")
    list(APPEND MGF_BLE_CONFIG_REQUIRES bt)
endif()
# GPIO38/GPIO40 replace microSD DAT0/CMD. Do not add an SDMMC owner here.
set(MGF_LILYGO_SDMMC_ENABLED 0 CACHE INTERNAL "SDMMC is unavailable in the K-line profile" FORCE)

idf_component_register(
    SRCS
        "app_main.c"
        "app/dashboard_controller.c"
        "app/settings_coordinator.c"
        "domain/app_settings.c"
        "domain/display_brightness.c"
        "domain/ble_config_protocol.c"
        "domain/mems_protocol.c"
        "domain/mems_reader.c"
        "domain/mems19_reader.c"
        "domain/mems_session.c"
        "domain/rtc_time.c"
        "infrastructure/board_display.c"
        "infrastructure/shared_i2c.c"
        "infrastructure/rtc_ds3231.c"
        "infrastructure/fake_ecu.c"
        "infrastructure/kline_uart_esp32.c"
        "infrastructure/mems_ecu.c"
        "infrastructure/settings_store.c"
        ${MGF_BLE_CONFIG_SRCS}
        "ui/icons/dash_icons.c"
        "ui/navigation/dashboard_navigator.c"
        "ui/navigation/ui_instrumentation.c"
        "ui/screens/style_amber.c"
        "ui/screens/boot_screen.c"
        "ui/screens/clock_screen.c"
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
        "ui/widgets/amber_text.c"
        "ui/fonts/ui_fonts.c"
    INCLUDE_DIRS "." "app" "domain" "infrastructure" "ui" "ui/screens" "ui/widgets" "ui/icons" "ui/fonts" "ui/themes"
    REQUIRES
        driver                        # GPIO + ESP-IDF compatibility types
        nvs_flash                     # persistent app settings and BT stack data
        ${MGF_BLE_CONFIG_REQUIRES}
        esp_lcd
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
)
