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
    "$ROOT/test/test_mems.c" \
    "$ROOT/main/domain/mems_protocol.c" \
    "$ROOT/main/domain/mems_reader.c" \
    "$ROOT/main/domain/mems19_reader.c" \
    "$ROOT/main/domain/mems_session.c" \
    -lm -o "$BUILD_DIR/test_mems"
"$BUILD_DIR/test_mems"
