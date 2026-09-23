#!/usr/bin/env bash
set -euo pipefail

LAUNCH="$HOME/.local/bin/quicklook-launch"

if [[ ! -x "$LAUNCH" ]]; then
    kdialog --error "QuickLook Linux no está instalado." 2>/dev/null || true
    exit 1
fi

if [[ "$#" -ge 1 ]]; then
    exec "$LAUNCH" "$1"
fi

FILE="$(kdialog --getopenfilename "$HOME" 2>/dev/null || true)"
[[ -n "$FILE" ]] || exit 0
exec "$LAUNCH" "$FILE"
