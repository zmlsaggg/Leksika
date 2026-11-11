#!/bin/sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
if ! command -v python3 >/dev/null 2>&1; then
    echo "нужен python3. Поставьте его и повторите." >&2
    exit 2
fi
exec python3 "$HERE/установить.lek" "$@"
