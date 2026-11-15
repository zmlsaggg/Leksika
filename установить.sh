#!/bin/sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
# Если это Git-клон, подтягиваем закреплённый источник словоформ до упаковки.
if [ -d "$HERE/.git" ] && command -v git >/dev/null 2>&1; then
    git -C "$HERE" submodule update --init --recursive || \
        echo "предупреждение: не удалось обновить подмодуль словаря; установка продолжится без него" >&2
fi
if ! command -v python3 >/dev/null 2>&1; then
    echo "нужен python3. Поставьте его и повторите." >&2
    exit 2
fi
exec python3 "$HERE/установить.lek" "$@"
