#!/usr/bin/env bash
# Check the dedicated ESP32-S3 IRAM budget after a successful IDF build.
#
# Usage:
#   tools/check_iram.sh
#   SKIP_BUILD=1 tools/check_iram.sh  # inspect the existing build/ artefacts
#
# IDF must be exported before invoking this script (or set IDF_PY to an
# absolute path). The default margin is deliberately non-zero: the current
# BLE-enabled image has only one byte left in the 16 KiB IRAM region.
# For the BLE image, point BUILD_DIR at its dedicated build directory and pass
# the same -B/-D options through IDF_PY_ARGS, e.g.:
#   BUILD_DIR=build-ble IDF_PY_ARGS='-B build-ble -D SDKCONFIG=build-ble/sdkconfig
#     -D SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ble
#     -D MGF_ENABLE_BLE_CONFIG=1' tools/check_iram.sh
set -euo pipefail

PROJECT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$PROJECT_DIR/build"}
APP_NAME=${APP_NAME:-mgf_gauge_lvgl}
IDF_PY=${IDF_PY:-idf.py}
IRAM_MIN_REMAIN_BYTES=${IRAM_MIN_REMAIN_BYTES:-256}
read -r -a IDF_PY_EXTRA <<<"${IDF_PY_ARGS:-}"

if [[ ${SKIP_BUILD:-0} != 1 ]]; then
    "$IDF_PY" -C "$PROJECT_DIR" "${IDF_PY_EXTRA[@]}" build
fi

map_file="$BUILD_DIR/$APP_NAME.map"
elf_file="$BUILD_DIR/$APP_NAME.elf"
if [[ ! -r "$map_file" || ! -r "$elf_file" ]]; then
    printf 'IRAM check: missing %s or %s; build the project first.\n' \
        "$map_file" "$elf_file" >&2
    exit 2
fi

size_json=$(mktemp)
trap 'rm -f "$size_json"' EXIT
"$IDF_PY" -C "$PROJECT_DIR" "${IDF_PY_EXTRA[@]}" size --format json --output-file "$size_json" >/dev/null

python3 - "$size_json" "$IRAM_MIN_REMAIN_BYTES" <<'PY'
import json
import sys

path, minimum = sys.argv[1], int(sys.argv[2])
with open(path, encoding="utf-8") as stream:
    size = json.load(stream)

required = ("used_iram", "iram_total", "iram_remain")
missing = [key for key in required if key not in size]
if missing:
    raise SystemExit(f"IRAM check: missing fields in idf.py size JSON: {', '.join(missing)}")

used = int(size["used_iram"])
total = int(size["iram_total"])
remain = int(size["iram_remain"])
percent = 100.0 * used / total if total else 0.0
print(f"IRAM: {used}/{total} bytes ({percent:.2f}%), remaining {remain}")

if remain < minimum:
    raise SystemExit(
        f"IRAM check failed: {remain} bytes remain, "
        f"minimum is {minimum}; inspect the map before adding IRAM-safe code."
    )
PY
