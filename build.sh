#!/usr/bin/env bash
# Compilation + régénération des goldens en une commande.
#
#   ./build.sh          compile l'UI et régénère test/golden/*_amber.png
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

CFLAGS=(-O2 -w -I"$SIM" -I"$MAIN" -I"$LVGL_DIR" -I"$LVGL_DIR/src" -DLV_CONF_INCLUDE_SIMPLE)

# Sources UI partagées avec la cible ESP32-S3 ; elles ne dépendent que de LVGL.
UI_SRCS=(
    "$SIM/main_sim.c"
    "$MAIN/app/dashboard_controller.c" "$MAIN/ui/navigation/dashboard_navigator.c"
    "$MAIN/ui/navigation/ui_instrumentation.c"
    "$MAIN/ui/screens/boot_screen.c" "$MAIN/infrastructure/fake_ecu.c"
    "$MAIN/ui/icons/dash_icons.c"
    "$MAIN/ui/screens/style_amber.c" "$MAIN/ui/themes/ui_theme.c"
    "$MAIN/ui/screens/clock_screen.c" "$MAIN/ui/screens/music_screen.c" "$MAIN/ui/screens/navigation_screen.c"
    "$MAIN/ui/screens/faults_screen.c" "$MAIN/ui/screens/temps_screen.c" "$MAIN/ui/screens/injection_screen.c"
    "$MAIN/ui/screens/lambda_screen.c" "$MAIN/ui/screens/ignition_screen.c"
    "$MAIN/ui/screens/idle_screen.c" "$MAIN/ui/screens/admission_screen.c"
    "$MAIN/ui/screens/settings_screen.c"
    "$MAIN/domain/app_settings.c" "$MAIN/domain/display_brightness.c"
    "$MAIN/domain/companion_protocol.c" "$MAIN/domain/companion_art.c"
    "$MAIN/domain/music_cover.c" "$MAIN/domain/flat_json.c"
    "$MAIN/infrastructure/jpeg_cover.c"
    "$MAIN/domain/rtc_time.c"
    "$MAIN/ui/widgets/amber_value.c" "$MAIN/ui/widgets/amber_draw.c" "$MAIN/ui/widgets/amber_ui.c" "$MAIN/ui/widgets/amber_kit.c" "$MAIN/ui/fonts/ui_fonts.c"
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
    -DMGF_SIMULATOR=1 -DTTF_PATH="\"$MAIN/fonts/Michroma-Regular.ttf\"" \
    "${UI_SRCS[@]}" "${LVGL_OBJS[@]}" \
    -lm -o "$BUILD/gen_golden"

# 4. Régénération des références visuelles (états nominaux).
echo ">> génération des goldens standard..."
"$BUILD/gen_golden" boot  "$GDIR/boot_amber.png"
"$BUILD/gen_golden" amber "$GDIR/rpm_amber.png"
for style in dashboard clock music navigation faults temps injection lambda ignition idle admission settings; do
    "$BUILD/gen_golden" "$style" "$GDIR/${style}_amber.png"
done

# 5. États réels et régressions : convention <écran>_<scénario>_amber.png
#    (une entrée par PNG, scénario passé en 3e argument).
echo ">> génération des goldens scénarios..."
# ECU déconnectée : toutes les vues télémétrie et services affichent « -- ».
"$BUILD/gen_golden" amber      "$GDIR/rpm_offline_amber.png"             offline
"$BUILD/gen_golden" faults     "$GDIR/faults_offline_amber.png"          offline
"$BUILD/gen_golden" temps      "$GDIR/temps_offline_amber.png"           offline
"$BUILD/gen_golden" injection  "$GDIR/injection_offline_amber.png"       offline
"$BUILD/gen_golden" lambda     "$GDIR/lambda_offline_amber.png"          offline
"$BUILD/gen_golden" ignition   "$GDIR/ignition_offline_amber.png"        offline
"$BUILD/gen_golden" idle       "$GDIR/idle_offline_amber.png"            offline
"$BUILD/gen_golden" admission  "$GDIR/admission_offline_amber.png"       offline
"$BUILD/gen_golden" music      "$GDIR/music_offline_amber.png"           offline
"$BUILD/gen_golden" navigation "$GDIR/navigation_offline_amber.png"      offline
"$BUILD/gen_golden" settings   "$GDIR/settings_offline_amber.png"        offline
# États capteurs : mesure absente, surchauffe, sonde d'eau en défaut.
"$BUILD/gen_golden" temps      "$GDIR/temps_missing_amber.png"           missing
"$BUILD/gen_golden" temps      "$GDIR/temps_hot_amber.png"               hot
"$BUILD/gen_golden" temps      "$GDIR/temps_sensor_fault_amber.png"      sensor_fault
# Unités impériales (vues qui gèrent set_units).
"$BUILD/gen_golden" amber      "$GDIR/rpm_imperial_amber.png"            imperial
"$BUILD/gen_golden" temps      "$GDIR/temps_imperial_amber.png"          imperial
"$BUILD/gen_golden" admission  "$GDIR/admission_imperial_amber.png"      imperial
# Reconnexions : transitions valid -> invalid -> valid / online -> offline -> online.
"$BUILD/gen_golden" amber      "$GDIR/rpm_reconnected_amber.png"         reconnected
"$BUILD/gen_golden" clock      "$GDIR/clock_reconnected_amber.png"       reconnected
# Horloge sans source de temps fiable.
"$BUILD/gen_golden" clock      "$GDIR/clock_unsynced_amber.png"          unsynced
# Textes longs (ellipsis) et lecture en pause.
"$BUILD/gen_golden" music      "$GDIR/music_long_amber.png"              long
"$BUILD/gen_golden" music      "$GDIR/music_paused_amber.png"            paused
"$BUILD/gen_golden" navigation "$GDIR/navigation_long_amber.png"         long
# Tableau de bord : alerte thermique globale posée sur la page NAVIGATION.
"$BUILD/gen_golden" dashboard  "$GDIR/dashboard_hot_amber.png"           hot
echo ">> OK — goldens régénérés dans test/golden/"
