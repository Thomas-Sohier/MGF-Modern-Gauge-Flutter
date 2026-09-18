#!/usr/bin/env bash
# Construit le simulateur hôte et régénère le golden de l'écran RPM.
#
# Compile la MÊME UI que la cible (main/dual_arc_dial.c + main/rpm_screen.c +
# main/fake_ecu.c) contre LVGL 9.x sur l'hôte, rend une frame offscreen avec des
# données mock et exporte test/golden/rpm_screen.png.
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
    "$MAIN_DIR/dual_arc_dial.c" "$MAIN_DIR/rpm_screen.c" "$MAIN_DIR/fake_ecu.c" \
    "$MAIN_DIR/gauge_icons.c" "$MAIN_DIR/dash_icons.c" \
    "$MAIN_DIR/style_amber.c" "$MAIN_DIR/style_cream.c" \
    "$MAIN_DIR/ui_fonts.c" \
    "${LVGL_SRCS[@]}" \
    -lm -o "$BUILD_DIR/gen_golden"

GDIR="$(dirname "$GOLDEN")"
"$BUILD_DIR/gen_golden" dual  "$GDIR/rpm_screen.png"
"$BUILD_DIR/gen_golden" amber "$GDIR/rpm_amber.png"
"$BUILD_DIR/gen_golden" cream "$GDIR/rpm_cream.png"
