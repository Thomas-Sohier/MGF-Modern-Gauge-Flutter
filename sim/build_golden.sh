#!/usr/bin/env bash
# Reconstruit le simulateur hôte SANS cache (LVGL recompilé intégralement) et
# régénère tous les goldens. Même liste de sources que ./build.sh, qui reste
# l'entrée recommandée pour itérer (LVGL mis en cache).
set -euo pipefail

SIM_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
exec "$(dirname "$SIM_DIR")/build.sh" clean
