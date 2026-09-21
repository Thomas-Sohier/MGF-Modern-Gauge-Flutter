#!/usr/bin/env bash
# Construit le simulateur hôte et régénère les goldens des écrans RPM et boot.
#
# Compile la même UI LVGL que la cible (avec les variantes legacy dual/cream)
# contre LVGL 9.x sur l'hôte, rend des frames offscreen avec des données mock et
# exporte les quatre goldens dans test/golden/.
#
#   LVGL_DIR : chemin vers des sources LVGL 9.x. Si absent, LVGL v9.2.2 est
#              cloné dans sim/.lvgl.
#
# NB : on compile directement les .c de LVGL (pas via son CMake), pour éviter
# le fichier assembleur NEON (ARM) qui ne s'assemble pas sur un hôte x86.
set -euo pipefail

SIM_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJ_DIR="$(dirname "$SIM_DIR")"
MAIN_DIR="$PROJ_DIR/main"
BUILD_DIR="$SIM_DIR/build"
GOLDEN="$PROJ_DIR/test/golden/rpm_screen.png"

: "${LVGL_DIR:=$SIM_DIR/.lvgl}"
if [ ! -d "$LVGL_DIR" ]; then
    echo ">> clone LVGL v9.2.2 -> $LVGL_DIR"
    git clone --depth 1 --branch v9.2.2 https://github.com/lvgl/lvgl.git "$LVGL_DIR"
fi

mkdir -p "$BUILD_DIR" "$(dirname "$GOLDEN")"

mapfile -t LVGL_SRCS < <(find "$LVGL_DIR/src" -name '*.c')
echo ">> compilation (${#LVGL_SRCS[@]} fichiers LVGL + UI)..."
gcc -O2 -w \
    -I"$SIM_DIR" -I"$MAIN_DIR" -I"$LVGL_DIR" -DLV_CONF_INCLUDE_SIMPLE \
    -DTTF_PATH="\"$MAIN_DIR/fonts/Michroma-Regular.ttf\"" \
    "$SIM_DIR/main_sim.c" \
    "$MAIN_DIR/app/dashboard_controller.c" \
    "$MAIN_DIR/ui/widgets/dual_arc_dial.c" "$MAIN_DIR/ui/screens/rpm_screen.c" "$MAIN_DIR/ui/screens/boot_screen.c" "$MAIN_DIR/infrastructure/fake_ecu.c" \
    "$MAIN_DIR/ui/icons/gauge_icons.c" "$MAIN_DIR/ui/icons/dash_icons.c" \
    "$MAIN_DIR/ui/screens/style_amber.c" "$MAIN_DIR/ui/screens/style_cream.c" "$MAIN_DIR/ui/themes/ui_theme.c" \
    "$MAIN_DIR/ui/widgets/amber_value.c" \
    "$MAIN_DIR/ui/fonts/ui_fonts.c" \
    "${LVGL_SRCS[@]}" \
    -lm -o "$BUILD_DIR/gen_golden"

GDIR="$(dirname "$GOLDEN")"
"$BUILD_DIR/gen_golden" dual  "$GDIR/rpm_screen.png"
"$BUILD_DIR/gen_golden" amber "$GDIR/rpm_amber.png"
"$BUILD_DIR/gen_golden" cream "$GDIR/rpm_cream.png"
"$BUILD_DIR/gen_golden" boot  "$GDIR/boot_amber.png"
