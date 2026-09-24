#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/test/.build"
mkdir -p "$BUILD_DIR"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_domain.c" -o "$BUILD_DIR/test_domain"
"$BUILD_DIR/test_domain"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_app_settings.c" \
    "$ROOT/main/domain/app_settings.c" -o "$BUILD_DIR/test_app_settings"
"$BUILD_DIR/test_app_settings"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT/test/mocks" -I"$ROOT/main" \
    "$ROOT/test/test_settings_store.c" \
    "$ROOT/main/infrastructure/settings_store.c" \
    "$ROOT/main/domain/app_settings.c" \
    -o "$BUILD_DIR/test_settings_store"
"$BUILD_DIR/test_settings_store"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_display_brightness.c" \
    "$ROOT/main/domain/display_brightness.c" -o "$BUILD_DIR/test_display_brightness"
"$BUILD_DIR/test_display_brightness"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_settings_coordinator.c" \
    "$ROOT/main/app/settings_coordinator.c" \
    "$ROOT/main/domain/app_settings.c" -o "$BUILD_DIR/test_settings_coordinator"
"$BUILD_DIR/test_settings_coordinator"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_ble_config_protocol.c" \
    "$ROOT/main/domain/ble_config_protocol.c" \
    "$ROOT/main/domain/app_settings.c" \
    "$ROOT/main/domain/rtc_time.c" -o "$BUILD_DIR/test_ble_config_protocol"
"$BUILD_DIR/test_ble_config_protocol"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_companion.c" \
    "$ROOT/main/domain/companion_protocol.c" \
    "$ROOT/main/domain/flat_json.c" \
    "$ROOT/main/domain/rtc_time.c" -lm -o "$BUILD_DIR/test_companion"
"$BUILD_DIR/test_companion"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_ble_window.c" \
    "$ROOT/main/domain/ble_window.c" -o "$BUILD_DIR/test_ble_window"
"$BUILD_DIR/test_ble_window"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_ui_layout.c" -lm -o "$BUILD_DIR/test_ui_layout"
"$BUILD_DIR/test_ui_layout"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_dashboard_timing.c" -o "$BUILD_DIR/test_dashboard_timing"
"$BUILD_DIR/test_dashboard_timing"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_value_smoothing.c" -lm -o "$BUILD_DIR/test_value_smoothing"
"$BUILD_DIR/test_value_smoothing"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_runtime_diagnostics.c" -o "$BUILD_DIR/test_runtime_diagnostics"
"$BUILD_DIR/test_runtime_diagnostics"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_mems.c" \
    "$ROOT/main/domain/mems_protocol.c" \
    "$ROOT/main/domain/mems_reader.c" \
    "$ROOT/main/domain/mems19_reader.c" \
    "$ROOT/main/domain/mems_session.c" \
    -lm -o "$BUILD_DIR/test_mems"
"$BUILD_DIR/test_mems"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT/test/kline_mocks" -I"$ROOT/main" \
    "$ROOT/test/test_kline_uart.c" \
    "$ROOT/main/infrastructure/kline_uart_esp32.c" \
    -o "$BUILD_DIR/test_kline_uart"
"$BUILD_DIR/test_kline_uart"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_kline_config.c" -o "$BUILD_DIR/test_kline_config"
"$BUILD_DIR/test_kline_config"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    -DKLINE_CONFIG_EXPECT_TX=22 -DKLINE_CONFIG_EXPECT_RX=23 \
    -DMGF_KLINE_TX_GPIO=22 -DMGF_KLINE_RX_GPIO=23 \
    "$ROOT/test/test_kline_config.c" -o "$BUILD_DIR/test_kline_config_override"
"$BUILD_DIR/test_kline_config_override"

# Known LILYGO display/I2C resources must fail at compile time.
for bad_gpio in 17 18 8 34 38 48; do
    if gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
        -DMGF_KLINE_TX_GPIO="$bad_gpio" -DMGF_KLINE_RX_GPIO=22 \
        "$ROOT/test/test_kline_config.c" -o "$BUILD_DIR/test_kline_config_invalid" \
        >"$BUILD_DIR/test_kline_config_invalid.log" 2>&1; then
        echo "expected GPIO$bad_gpio resource conflict to fail" >&2
        exit 1
    fi
done

for bad_define in \
    "-DMGF_USE_MEMS_KLINE=2" \
    "-DMGF_KLINE_LOCAL_ECHO=2" \
    "-DMGF_LILYGO_SDMMC_ENABLED=1"; do
    if gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
        "$bad_define" "$ROOT/test/test_kline_config.c" \
        -o "$BUILD_DIR/test_kline_config_invalid_define" \
        >"$BUILD_DIR/test_kline_config_invalid_define.log" 2>&1; then
        echo "expected invalid K-line define $bad_define to fail" >&2
        exit 1
    fi
done

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_rtc.c" \
    "$ROOT/main/domain/rtc_time.c" \
    -o "$BUILD_DIR/test_rtc"
"$BUILD_DIR/test_rtc"

gcc -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT/test/mocks" -I"$ROOT/main" \
    "$ROOT/test/test_rtc_ds3231.c" \
    "$ROOT/main/infrastructure/rtc_ds3231.c" \
    "$ROOT/main/domain/rtc_time.c" \
    -o "$BUILD_DIR/test_rtc_ds3231"
"$BUILD_DIR/test_rtc_ds3231"
