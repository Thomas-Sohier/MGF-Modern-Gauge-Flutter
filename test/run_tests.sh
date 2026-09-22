#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT/test/.build"
mkdir -p "$BUILD_DIR"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_domain.c" -o "$BUILD_DIR/test_domain"
"$BUILD_DIR/test_domain"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_ui_layout.c" -lm -o "$BUILD_DIR/test_ui_layout"
"$BUILD_DIR/test_ui_layout"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_dashboard_timing.c" -o "$BUILD_DIR/test_dashboard_timing"
"$BUILD_DIR/test_dashboard_timing"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_mems.c" \
    "$ROOT/main/domain/mems_protocol.c" \
    "$ROOT/main/domain/mems_reader.c" \
    "$ROOT/main/domain/mems19_reader.c" \
    "$ROOT/main/domain/mems_session.c" \
    -lm -o "$BUILD_DIR/test_mems"
"$BUILD_DIR/test_mems"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    "$ROOT/test/test_kline_config.c" -o "$BUILD_DIR/test_kline_config"
"$BUILD_DIR/test_kline_config"

gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
    -DKLINE_CONFIG_EXPECT_TX=22 -DKLINE_CONFIG_EXPECT_RX=23 \
    -DMGF_KLINE_TX_GPIO=22 -DMGF_KLINE_RX_GPIO=23 \
    "$ROOT/test/test_kline_config.c" -o "$BUILD_DIR/test_kline_config_override"
"$BUILD_DIR/test_kline_config_override"

# Known LILYGO display/I2C resources must fail at compile time.  This catches
# accidental regressions in the board resource policy without requiring ESP-IDF.
for bad_gpio in 17 18 8 48; do
    if gcc -std=c11 -Wall -Wextra -Werror -I"$ROOT/main" \
        -DMGF_KLINE_TX_GPIO="$bad_gpio" -DMGF_KLINE_RX_GPIO=22 \
        "$ROOT/test/test_kline_config.c" -o "$BUILD_DIR/test_kline_config_invalid" \
        >"$BUILD_DIR/test_kline_config_invalid.log" 2>&1; then
        echo "expected GPIO$bad_gpio resource conflict to fail" >&2
        exit 1
    fi
done
