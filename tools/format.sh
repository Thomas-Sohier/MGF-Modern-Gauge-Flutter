#!/usr/bin/env bash
# Formatage des sources C/H du projet.
#
#   tools/format.sh          applique clang-format en place
#   tools/format.sh check    vérifie sans modifier (CI) ; échoue si un fichier
#                            n'est pas conforme
#
# Les sources tierces (managed_components, spécifications upstream, stb) sont
# exclues. Version de référence : clang-format 23.1.1 (cf. .clang-format et la
# CI). Surcharger le binaire avec CLANG_FORMAT=/chemin/vers/clang-format.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLANG_FORMAT=${CLANG_FORMAT:-clang-format}

cd "$ROOT"
mapfile -t FILES < <(
    git ls-files '*.c' '*.h' |
        grep -v -E '^(managed_components/|specs/|sim/stb_image_write\.h$)'
)

if [ "${#FILES[@]}" -eq 0 ]; then
    echo "format: aucun fichier à traiter" >&2
    exit 2
fi

if [ "${1:-}" = "check" ]; then
    "$CLANG_FORMAT" --dry-run --Werror "${FILES[@]}"
    echo "format: ${#FILES[@]} fichiers conformes"
else
    "$CLANG_FORMAT" -i "${FILES[@]}"
    echo "format: ${#FILES[@]} fichiers formatés"
fi
