# Firmware ESP32-S3 : seul l'écran ambre est embarqué (les styles dual/cream et
# leurs icônes ne servent qu'au simulateur hôte, cf. sim/).
idf_component_register(
    SRCS
        "app_main.c"
        "app/dashboard_controller.c"
        "domain/mems_protocol.c"
        "domain/mems_reader.c"
        "domain/mems19_reader.c"
        "domain/mems_session.c"
        "infrastructure/board_display.c"
        "infrastructure/fake_ecu.c"
        "infrastructure/kline_uart_esp32.c"
        "infrastructure/mems_ecu.c"
        "ui/icons/dash_icons.c"
        "ui/navigation/dashboard_navigator.c"
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
        driver                        # I²C (legacy) + GPIO
        esp_lcd
        esp_lcd_st7701
        esp_lcd_panel_io_additions    # SPI 3 fils (init ST7701) via expander
        esp_io_expander_tca9554
        esp_lcd_touch
        esp_lcd_touch_cst816s
        esp_lvgl_port
        lvgl
    EMBED_FILES "fonts/Michroma-Regular.ttf"
)
