#!/usr/bin/env bash
# Compilation + régénération des goldens en une commande.
#
#   ./build.sh          compile l'UI et régénère test/golden/{rpm_screen,rpm_amber,rpm_cream,boot_amber}.png
#   ./build.sh clean    repart de zéro (efface le cache sim/build/)
#
# LVGL n'est compilé qu'UNE fois (objets mis en cache dans sim/build/lvgl_obj/) ;
# les fois suivantes seule l'UI est recompilée -> rebuild quasi instantané.
#
#   LVGL_DIR : sources LVGL 9.x à utiliser. Absent -> clone v9.2.2 dans sim/.lvgl.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SIM="$ROOT/sim"
MAIN="$ROOT/main"
BUILD="$SIM/build"
GDIR="$ROOT/test/golden"
: "${LVGL_DIR:=$SIM/.lvgl}"
JOBS="$(nproc 2>/dev/null || echo 4)"

CFLAGS=(-O2 -w -I"$SIM" -I"$MAIN" -I"$LVGL_DIR" -DLV_CONF_INCLUDE_SIMPLE)

# Sources UI partagées avec la cible ESP32-S3 ; elles ne dépendent que de LVGL.
UI_SRCS=(
    "$SIM/main_sim.c"
    "$MAIN/app/dashboard_controller.c" "$MAIN/ui/navigation/dashboard_navigator.c"
    "$MAIN/ui/widgets/dual_arc_dial.c" "$MAIN/ui/screens/rpm_screen.c" "$MAIN/ui/screens/boot_screen.c" "$MAIN/infrastructure/fake_ecu.c"
    "$MAIN/ui/icons/gauge_icons.c" "$MAIN/ui/icons/dash_icons.c"
    "$MAIN/ui/screens/style_amber.c" "$MAIN/ui/screens/style_cream.c" "$MAIN/ui/themes/ui_theme.c"
    "$MAIN/ui/screens/clock_screen.c" "$MAIN/ui/screens/music_screen.c" "$MAIN/ui/screens/navigation_screen.c"
    "$MAIN/ui/screens/faults_screen.c" "$MAIN/ui/screens/temps_screen.c" "$MAIN/ui/screens/injection_screen.c"
    "$MAIN/ui/screens/lambda_screen.c" "$MAIN/ui/screens/ignition_screen.c"
    "$MAIN/ui/screens/idle_screen.c" "$MAIN/ui/screens/admission_screen.c"
    "$MAIN/ui/widgets/amber_value.c" "$MAIN/ui/fonts/ui_fonts.c"
)

if [ "${1:-}" = "clean" ]; then
    echo ">> clean : $BUILD"
    rm -rf "$BUILD"
fi

# 1. Sources LVGL (clonées au besoin).
if [ ! -d "$LVGL_DIR" ]; then
    echo ">> clone LVGL v9.2.2 -> $LVGL_DIR"
    git clone --depth 1 --branch v9.2.2 https://github.com/lvgl/lvgl.git "$LVGL_DIR"
fi

OBJ="$BUILD/lvgl_obj"
mkdir -p "$OBJ" "$GDIR"

# 2. LVGL compilé une seule fois -> objets en cache (recompilés en parallèle).
export CC_CFLAGS="${CFLAGS[*]}"
export OBJ_DIR="$OBJ"
compile_one() {
    local src="$1"
    local obj="$OBJ_DIR/$(printf '%s' "$src" | tr '/.' '__').o"
    [ -f "$obj" ] && return 0
    gcc $CC_CFLAGS -c "$src" -o "$obj"
}
export -f compile_one

mapfile -t LVGL_SRCS < <(find "$LVGL_DIR/src" -name '*.c')
need_lvgl=0
for s in "${LVGL_SRCS[@]}"; do
    [ -f "$OBJ/$(printf '%s' "$s" | tr '/.' '__').o" ] || { need_lvgl=1; break; }
done

if [ "$need_lvgl" = 1 ]; then
    echo ">> compilation LVGL (${#LVGL_SRCS[@]} fichiers, $JOBS jobs, cache une fois)..."
    printf '%s\n' "${LVGL_SRCS[@]}" | xargs -P"$JOBS" -I{} bash -c 'compile_one "$1"' _ {}
else
    echo ">> LVGL déjà en cache ($OBJ)"
fi

mapfile -t LVGL_OBJS < <(find "$OBJ" -name '*.o')

# 3. UI + simulateur (recompilés à chaque fois), liés aux objets LVGL du cache.
echo ">> compilation UI + simulateur..."
gcc "${CFLAGS[@]}" \
    -DTTF_PATH="\"$MAIN/fonts/Michroma-Regular.ttf\"" \
    "${UI_SRCS[@]}" "${LVGL_OBJS[@]}" \
    -lm -o "$BUILD/gen_golden"

# 4. Régénération des références visuelles.
echo ">> génération des goldens..."
"$BUILD/gen_golden" dual  "$GDIR/rpm_screen.png"
"$BUILD/gen_golden" cream "$GDIR/rpm_cream.png"
"$BUILD/gen_golden" boot  "$GDIR/boot_amber.png"
"$BUILD/gen_golden" amber "$GDIR/rpm_amber.png"
for style in clock music navigation faults temps injection lambda ignition idle admission; do
    "$BUILD/gen_golden" "$style" "$GDIR/${style}_amber.png"
done
echo ">> OK — goldens régénérés dans test/golden/"
