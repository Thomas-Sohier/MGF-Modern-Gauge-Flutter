idf_component_register(
    SRCS
        "app_main.c"
        "fake_ecu.c"
        "dual_arc_dial.c"
        "rpm_screen.c"
        "gauge_icons.c"
        "dash_icons.c"
        "style_amber.c"
        "style_cream.c"
        "ui_fonts.c"
    INCLUDE_DIRS "."
    EMBED_FILES "fonts/JetBrainsMono-Bold.ttf"
)
