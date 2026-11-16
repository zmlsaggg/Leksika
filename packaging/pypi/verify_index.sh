#!/usr/bin/env bash
# Сверяет файлы из dist/ с тем, что индекс уже отдаёт по JSON API.
# Ошибка загрузки на PyPI возможна и после успешного ответа, поэтому
# публикация не считается законченной, пока суммы не совпали.
#
# Переменные: VERSION (например 0.4.0b1), INDEX (pypi.org или test.pypi.org),
# DIST — каталог с файлами, по умолчанию dist.
set -euo pipefail
: "${VERSION:?нужна переменная VERSION}"
: "${INDEX:?нужна переменная INDEX}"
DIST="${DIST:-dist}"

python3 - "$VERSION" "$INDEX" "$DIST" <<'ПИТОН'
import hashlib
import json
import pathlib
import sys
import time
import urllib.error
import urllib.request

версия, индекс, каталог = sys.argv[1], sys.argv[2], pathlib.Path(sys.argv[3])
колесо = sorted(каталог.glob("*.whl"))
архив = sorted(каталог.glob("*.tar.gz"))
if len(колесо) != 1 or len(архив) != 1:
    raise SystemExit(f"ожидались один wheel и один sdist, найдено: {колесо} {архив}")

местные = {путь.name: hashlib.sha256(путь.read_bytes()).hexdigest() for путь in колесо + архив}
адрес = f"https://{индекс}/pypi/leksika/{версия}/json"

данные = None
for попытка in range(40):
    try:
        with urllib.request.urlopen(адрес, timeout=30) as ответ:
            данные = json.load(ответ)
        break
    except urllib.error.HTTPError as ошибка:
        if ошибка.code != 404:  # 404 — индекс ещё не обновился
            raise
    except OSError:
        pass
    time.sleep(15)
if данные is None:
    raise SystemExit(f"файлы не появились в индексе за отведённое время: {адрес}")

опубликовано = {файл["filename"]: файл["digests"]["sha256"] for файл in данные.get("urls", [])}
for имя, сумма in местные.items():
    if опубликовано.get(имя) != сумма:
        raise SystemExit(
            f"сумма {имя} не совпала: локально {сумма}, в индексе {опубликовано.get(имя)}"
        )
print(f"{индекс}: leksika {версия} — файлов {len(местные)}, SHA-256 совпадают")
print(адрес)
ПИТОН
