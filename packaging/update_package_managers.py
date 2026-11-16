#!/usr/bin/env python3
"""Обновляет внешние пакетные менеджеры версией, ссылками и SHA-256 релиза.

Скрипт правит два файла в склонированных репозиториях:

* ``homebrew-leksika/Formula/leksika.rb`` — ``version`` плюс ``url``/``sha256``
  для macOS arm64, macOS x64 и Linux x64;
* ``scoop-leksika/bucket/leksika.json`` — ``version``, ``url`` и ``hash``
  для Windows x64.

Задача — точечная замена: остальная структура файлов, комментарии и порядок
ключей сохраняются. Поэтому формула обрабатывается построчно регулярными
выражениями, а манифест Scoop читается и пишется как JSON с тем же отступом.

Суммы берутся из опубликованного ``zz-SHA256SUMS.txt``: файл можно передать
локально или разрешить скачать из GitHub Release нужного тега. Без сети и без
учётных данных скрипт тоже работает — ему достаточно локального файла сумм.

Примеры::

    python3 packaging/update_package_managers.py 1.0.0 \\
        --sums zz-SHA256SUMS.txt \\
        --homebrew путь/к/homebrew-leksika --scoop путь/к/scoop-leksika

    python3 packaging/update_package_managers.py 1.0.0 --download --homebrew ...

Коды возврата: 0 — файлы обновлены (или уже были актуальны), 1 — проверка
входа не прошла, 2 — ошибка ввода-вывода. Скрипт ничего не коммитит и не
отправляет: это делает вызывающий workflow.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import urllib.error
import urllib.request
from pathlib import Path

ВЕРСИЯ_RE = re.compile(r"(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*)")

# Архивы, которые должны быть в релизе. Порядок задаёт разбор аргументов
# позиций в формуле Homebrew (macOS arm64, macOS x64, Linux x64).
АРХИВЫ = {
    "macos-arm64": "leksika-native-macos-arm64.tar.gz",
    "macos-x64": "leksika-native-macos-x64.tar.gz",
    "linux-x64": "leksika-native-linux-x64.tar.gz",
    "windows-x64": "leksika-native-windows-x64.zip",
}
СУММА_RE = re.compile(r"^(?P<хэш>[0-9a-fA-F]{64})\s+\*?(?P<имя>\S+)$")


class ОшибкаПроверки(Exception):
    """Вход не соответствует ожиданиям; менять файлы нельзя."""


def прочитать_суммы(файл: Path) -> dict[str, str]:
    """Разбирает zz-SHA256SUMS.txt в словарь «имя файла → хеш» в нижнем регистре."""
    try:
        текст = файл.read_text(encoding="utf-8")
    except OSError as ошибка:
        raise ОшибкаПроверки(f"не удалось прочитать суммы {файл}: {ошибка}") from ошибка
    суммы: dict[str, str] = {}
    for номер, строка in enumerate(текст.splitlines(), 1):
        строка = строка.strip()
        if not строка or строка.startswith("#"):
            continue
        совпадение = СУММА_RE.match(строка)
        if not совпадение:
            raise ОшибкаПроверки(f"строка {номер} в {файл.name} не похожа на SHA-256: {строка!r}")
        имя = совпадение.group("имя")
        хэш = совпадение.group("хэш").lower()
        if имя in суммы and суммы[имя] != хэш:
            raise ОшибкаПроверки(f"в {файл.name} две разные суммы для {имя}")
        суммы[имя] = хэш
    отсутствуют = [имя for имя in АРХИВЫ.values() if имя not in суммы]
    if отсутствуют:
        raise ОшибкаПроверки(
            "в zz-SHA256SUMS.txt нет сумм для архивов: " + ", ".join(отсутствуют)
        )
    return суммы


def адрес_релиза(версия: str, архив: str) -> str:
    return (
        "https://github.com/zmlsaggg/Leksika/releases/download/"
        f"v{версия}/{архив}"
    )


def скачать_суммы(версия: str, куда: Path) -> Path:
    """Скачивает zz-SHA256SUMS.txt из GitHub Release без токена.

    Релиз публичный, поэтому авторизация не нужна; GH_TOKEN в признак
    подлинности не добавляет, а лишний секрет в этом шаге не нужен.
    """
    адрес = адрес_релиза(версия, "zz-SHA256SUMS.txt")
    print(f"Скачиваю суммы: {адрес}")
    try:
        with urllib.request.urlopen(адрес, timeout=60) as ответ:
            данные = ответ.read()
    except urllib.error.HTTPError as ошибка:
        raise ОшибкаПроверки(
            f"не удалось скачать {адрес}: HTTP {ошибка.code}. "
            "Релиз опубликован? Файл zz-SHA256SUMS.txt загружен?"
        ) from ошибка
    except urllib.error.URLError as ошибка:
        raise ОшибкаПроверки(f"нет сети или GitHub недоступен: {ошибка.reason}") from ошибка
    if not данные:
        raise ОшибкаПроверки(f"пустой ответ от {адрес}")
    куда.parent.mkdir(parents=True, exist_ok=True)
    куда.write_bytes(данные)
    return куда


def обновить_формулу(текст: str, версия: str, sums: dict[str, str]) -> str:
    """Правит Formula/leksika.rb, сохраняя комментарии и порядок строк.

    Сумма привязывается к архиву из ближайшей предшествующей строки ``url``,
    а не к порядку блоков: если в формуле переставят macOS и Linux, суммы всё
    равно останутся при своих архивах. Незнакомый архив в url — ошибка, а не
    молчаливая подмена.
    """
    строки = текст.splitlines(keepends=True)
    было = len(строки)
    новых: list[str] = []
    без_версии = True
    текущий_архив: str | None = None
    заполнено: set[str] = set()

    for номер, строка in enumerate(строки):
        хвост_перевода = строка[len(строка.rstrip("\n")):]

        совпадение_версии = re.match(r'^(\s*version\s+)"[^"]*"(.*)$', строка.rstrip("\n"))
        if совпадение_версии:
            новых.append(f'{совпадение_версии.group(1)}"{версия}"{совпадение_версии.group(2)}{хвост_перевода}')
            без_версии = False
            continue

        совпадение_url = re.search(
            r'"(https://github\.com/zmlsaggg/Leksika/releases/download/[^"]*'
            r'(leksika-native-(?:macos-arm64|macos-x64|linux-x64|windows-x64)'
            r'\.(?:tar\.gz|zip)))"',
            строка,
        )
        if совпадение_url:
            текущий_архив = совпадение_url.group(2)
            новая = (
                строка[: совпадение_url.start()]
                + f'"{адрес_релиза(версия, текущий_архив)}"'
                + строка[совпадение_url.end():]
            )
            новых.append(новая)
            continue

        совпадение_sha = re.match(r'^(\s*sha256\s+)"[0-9a-fA-F]{64}"(.*)$', строка.rstrip("\n"))
        if совпадение_sha:
            if текущий_архив is None:
                raise ОшибкаПроверки(
                    f"строка {номер + 1}: sha256 идёт раньше url — непонятно, к какому архиву она относится"
                )
            if текущий_архив in заполнено:
                raise ОшибкаПроверки(
                    f"строка {номер + 1}: у архива {текущий_архив} уже была сумма"
                )
            хэш = sums[текущий_архив]
            заполнено.add(текущий_архив)
            новых.append(
                f'{совпадение_sha.group(1)}"{хэш}"{совпадение_sha.group(2)}{хвост_перевода}'
            )
            текущий_архив = None
            continue

        новых.append(строка)

    if без_версии:
        raise ОшибкаПроверки('в формуле не найдена строка version "..."')
    ожидалось = {АРХИВЫ["macos-arm64"], АРХИВЫ["macos-x64"], АРХИВЫ["linux-x64"]}
    недостаёт = ожидалось - заполнено
    if недостаёт:
        raise ОшибкаПроверки(
            "в формуле нет суммы для архивов: " + ", ".join(sorted(недостаёт))
        )
    if len(новых) != было:
        raise ОшибкаПроверки("внутренняя ошибка: число строк формулы изменилось")

    # Проверяем результат, а не намерение: все три суммы и версия на месте.
    готовый = "".join(новых)
    for имя in sorted(ожидалось):
        if f'sha256 "{sums[имя]}"' not in готовый:
            raise ОшибкаПроверки(f"после замены в формуле нет суммы для {имя}")
    if f'"{версия}"' not in готовый:
        raise ОшибкаПроверки("после замены в формуле нет нужной версии")
    return готовый


def обновить_манифест(текст: str, версия: str, sums: dict[str, str]) -> str:
    """Правит bucket/leksika.json точечно, сохраняя форматирование.

    json.dumps переформатировал бы вложенные массивы ``bin`` в многострочные,
    и дифф раздувался бы на весь файл. Поэтому меняются ровно три значения —
    version, url и hash у architecture.64bit, — а результат проверяется
    повторным разбором JSON.
    """
    try:
        исходный = json.loads(текст)
    except json.JSONDecodeError as ошибка:
        raise ОшибкаПроверки(f"bucket/leksika.json не разбирается как JSON: {ошибка}") from ошибка
    if not isinstance(исходный, dict):
        raise ОшибкаПроверки("bucket/leksika.json должен быть объектом")
    архитектуры = исходный.get("architecture")
    if not isinstance(архитектуры, dict) or not isinstance(архитектуры.get("64bit"), dict):
        raise ОшибкаПроверки('в манифесте нет объекта architecture."64bit"')

    if len(re.findall(r'"version"\s*:', текст)) != 1:
        raise ОшибкаПроверки('в манифесте должно быть одно поле "version"')

    # Границы блока 64bit: от его ключа до конца файла. Первые url и hash
    # после этой точки принадлежат именно этому блоку; autoupdate идёт ниже,
    # и его hash — объект, а не строка, поэтому под шаблон не попадает.
    место = текст.find('"64bit"')
    if место < 0:
        raise ОшибкаПроверки('в тексте манифеста нет ключа "64bit"')
    голова, хвост = текст[:место], текст[место:]

    версия_шаблон = re.compile(r'("version"\s*:\s*)"[^"]*"')
    url_шаблон = re.compile(r'("url"\s*:\s*)"[^"]*"')
    хэш_шаблон = re.compile(r'("hash"\s*:\s*)"[0-9a-fA-F]{64}"')

    if not версия_шаблон.search(голова + хвост):
        raise ОшибкаПроверки('не найдено поле "version"')
    хвост, сколько_url = url_шаблон.subn(r'\1"' + адрес_релиза(версия, АРХИВЫ["windows-x64"]) + '"', хвост, count=1)
    if сколько_url != 1:
        raise ОшибкаПроверки('в блоке 64bit нет поля "url"')
    хвост, сколько_хэш = хэш_шаблон.subn(r'\1"' + sums[АРХИВЫ["windows-x64"]] + '"', хвост, count=1)
    if сколько_хэш != 1:
        raise ОшибкаПроверки('в блоке 64bit нет строкового поля "hash" с SHA-256')

    готовый = версия_шаблон.sub(r'\1"' + версия + '"', голова, count=1) + хвост

    # Проверяем результат разбором: структура цела, значения верные.
    try:
        итог = json.loads(готовый)
    except json.JSONDecodeError as ошибка:
        raise ОшибкаПроверки(f"после замены манифест перестал быть JSON: {ошибка}") from ошибка
    if итог.get("version") != версия:
        raise ОшибкаПроверки("после замены версия в манифесте не совпадает")
    if итог["architecture"]["64bit"]["url"] != адрес_релиза(версия, АРХИВЫ["windows-x64"]):
        raise ОшибкаПроверки("после замены URL в манифесте не совпадает")
    if итог["architecture"]["64bit"]["hash"] != sums[АРХИВЫ["windows-x64"]]:
        raise ОшибкаПроверки("после замены хеш в манифесте не совпадает")
    if итог.get("autoupdate") != исходный.get("autoupdate"):
        raise ОшибкаПроверки("после замены пострадал блок autoupdate")
    return готовый


def записать_если_изменилось(файл: Path, содержимое: str) -> bool:
    """Пишет файл только при реальном изменении; возвращает True, если обновил."""
    try:
        старое = файл.read_text(encoding="utf-8")
    except FileNotFoundError as ошибка:
        raise ОшибкаПроверки(f"нет файла {файл}: сначала склонируйте репозиторий") from ошибка
    except OSError as ошибка:
        raise ОшибкаПроверки(f"не удалось прочитать {файл}: {ошибка}") from ошибка
    if старое == содержимое:
        print(f"Без изменений: {файл}")
        return False
    файл.write_text(содержимое, encoding="utf-8")
    print(f"Обновлён: {файл}")
    return True


def разобрать_аргументы(argv: list[str] | None = None) -> argparse.Namespace:
    разбор = argparse.ArgumentParser(
        description="Обновить homebrew-leksika и scoop-leksika данными релиза Лексики.",
        epilog="Суммы берутся из zz-SHA256SUMS.txt опубликованного релиза.",
    )
    разбор.add_argument("version", help="версия тега без префикса v, например 1.0.0")
    разбор.add_argument(
        "--sums",
        type=Path,
        help="путь к zz-SHA256SUMS.txt (обязателен, если не указан --download)",
    )
    разбор.add_argument(
        "--download",
        action="store_true",
        help="скачать zz-SHA256SUMS.txt из GitHub Release версии",
    )
    разбор.add_argument(
        "--cache-dir",
        type=Path,
        default=Path(os.environ.get("RUNNER_TEMP", "/tmp")) / "leksika-sums",
        help="куда сохранить скачанный файл сумм (по умолчанию RUNNER_TEMP)",
    )
    разбор.add_argument("--homebrew", type=Path, help="путь к клону homebrew-leksika")
    разбор.add_argument("--scoop", type=Path, help="путь к клону scoop-leksika")
    разбор.add_argument(
        "--сухой-прогон",
        "--dry-run",
        dest="сухой_прогон",
        action="store_true",
        help="проверить все входы и показать изменения, ничего не записывая",
    )
    разбор.add_argument("--verbose", action="store_true", help="печатать разобранные суммы")
    return разбор.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    аргументы = разобрать_аргументы(argv)
    try:
        if not ВЕРСИЯ_RE.fullmatch(аргументы.version):
            raise ОшибкаПроверки(
                f"версия должна быть X.Y.Z без префикса v, а не {аргументы.version!r}; "
                "предварительные версии в пакетные менеджеры не попадают"
            )
        if not аргументы.homebrew and not аргументы.scoop:
            raise ОшибкаПроверки("укажите --homebrew и/или --scoop — что именно обновлять")
        if аргументы.sums and аргументы.download:
            raise ОшибкаПроверки("--sums и --download взаимоисключающие")
        if not аргументы.sums and not аргументы.download:
            raise ОшибкаПроверки("нужен --sums путь или --download")

        файл_сумм = аргументы.sums
        if аргументы.download:
            файл_сумм = скачать_суммы(аргументы.version, аргументы.cache_dir / "zz-SHA256SUMS.txt")

        sums = прочитать_суммы(файл_сумм)
        if аргументы.verbose:
            for имя in АРХИВЫ.values():
                print(f"  {sums[имя]}  {имя}")

        изменения = False
        if аргументы.homebrew:
            формула = аргументы.homebrew / "Formula" / "leksika.rb"
            новое = обновить_формулу(прочитать_файл(формула), аргументы.version, sums)
            if аргументы.сухой_прогон:
                print(f"[сухой прогон] обновил бы {формула}")
            else:
                изменения |= записать_если_изменилось(формула, новое)
        if аргументы.scoop:
            манифест = аргументы.scoop / "bucket" / "leksika.json"
            новое = обновить_манифест(прочитать_файл(манифест), аргументы.version, sums)
            if аргументы.сухой_прогон:
                print(f"[сухой прогон] обновил бы {манифест}")
            else:
                изменения |= записать_если_изменилось(манифест, новое)
    except ОшибкаПроверки as ошибка:
        print(f"Ошибка: {ошибка}", file=sys.stderr)
        return 1
    except OSError as ошибка:
        print(f"Ошибка ввода-вывода: {ошибка}", file=sys.stderr)
        return 2

    if аргументы.сухой_прогон:
        print(f"Сухой прогон завершён: версию {аргументы.version} можно применять, файлы не тронуты.")
    elif изменения:
        print(f"Готово: пакетные менеджеры обновлены до {аргументы.version}.")
    else:
        print(f"Готово: пакетные менеджеры уже соответствуют версии {аргументы.version}.")
    return 0


def прочитать_файл(файл: Path) -> str:
    try:
        return файл.read_text(encoding="utf-8")
    except FileNotFoundError as ошибка:
        raise ОшибкаПроверки(f"нет файла {файл}: сначала склонируйте репозиторий") from ошибка
    except OSError as ошибка:
        raise ОшибкаПроверки(f"не удалось прочитать {файл}: {ошибка}") from ошибка


if __name__ == "__main__":
    sys.exit(main())
