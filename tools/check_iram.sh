#!/usr/bin/env bash
# Check internal RAM headroom after a successful IDF build.
#
# Usage:
#   tools/check_iram.sh
#   SKIP_BUILD=1 tools/check_iram.sh  # inspect the existing build/ artefacts
#
# On ESP32-S3 the linker always fills the 16 KiB "IRAM" region first and then
# places the remaining IRAM code in DIRAM, so `idf.py size` reports that region
# at ~100 % on every image: it is not a limit. The real budget is DIRAM (IRAM
# code + .data + .bss share it, and the heap gets what remains), which this
# script enforces. IRAM is printed for information only.
#
# IDF must be exported before invoking this script (or set IDF_PY to an
# absolute path). For the BLE image, target its dedicated build directory:
#   BUILD_DIR=build-ble IDF_PY_ARGS='-B build-ble -D SDKCONFIG=build-ble/sdkconfig
#     -D SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ble
#     -D MGF_ENABLE_BLE_CONFIG=1' tools/check_iram.sh
set -euo pipefail

PROJECT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-"$PROJECT_DIR/build"}
APP_NAME=${APP_NAME:-mgf_gauge_lvgl}
IDF_PY=${IDF_PY:-idf.py}
DIRAM_MIN_REMAIN_BYTES=${DIRAM_MIN_REMAIN_BYTES:-131072}
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

python3 - "$size_json" "$DIRAM_MIN_REMAIN_BYTES" <<'PY'
import json
import sys

path, minimum = sys.argv[1], int(sys.argv[2])
with open(path, encoding="utf-8") as stream:
    size = json.load(stream)

required = ("used_iram", "iram_total", "used_diram", "diram_total", "diram_remain")
missing = [key for key in required if key not in size]
if missing:
    raise SystemExit(f"RAM check: missing fields in idf.py size JSON: {', '.join(missing)}")

print(f"IRAM region: {size['used_iram']}/{size['iram_total']} bytes "
      "(always ~full on ESP32-S3; overflow goes to DIRAM)")
used = int(size["used_diram"])
total = int(size["diram_total"])
remain = int(size["diram_remain"])
percent = 100.0 * used / total if total else 0.0
print(f"DIRAM: {used}/{total} bytes ({percent:.2f}%), remaining {remain}")

if remain < minimum:
    raise SystemExit(
        f"RAM check failed: {remain} DIRAM bytes remain, minimum is {minimum}; "
        "inspect idf.py size-components before adding internal-RAM code/data."
    )
PY
