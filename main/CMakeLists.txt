# Firmware ESP32-S3 : seul l'écran ambre est embarqué (les styles dual/cream et
# leurs icônes ne servent qu'au simulateur hôte, cf. sim/).
idf_component_register(
    SRCS
        "app_main.c"
        "board_display.c"
        "fake_ecu.c"
        "dash_icons.c"
        "style_amber.c"
        "ui_fonts.c"
    INCLUDE_DIRS "."
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
