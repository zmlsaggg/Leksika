#!/usr/bin/env python3
"""Сверяет колесо с исходниками: в пакет не должно попадать «кастрированное».

Пакет на PyPI — это не только запускатор, но и весь движок: части на `.lek`,
примеры, программы, словари, документы и данные словаря. Если в
`pyproject.toml` забыть каталог, `pip install leksika` поставит урезанный
движок, и это заметит только пользователь. Скрипт сравнивает содержимое
каталогов с колесом и падает при любом расхождении.

Запуск: python3 packaging/pypi/проверить_состав.py dist/leksika-*.whl
"""

import glob
import os
import sys
import zipfile

КОРЕНЬ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Что обязано лежать в колесе целиком (пути от корня репозитория).
КАТАЛОГИ = [
    "лексика",
    "примеры",
    "программы",
    "словари",
    "документы",
    "данные/словарь",
]
ФАЙЛЫ = ["лексус.lek", "читай.lek"]

# Служебный мусор, который в пакет не берут.
ПРОПУСКАТЬ = ("__pycache__",)


def исходники():
    итог = set()
    for файл in ФАЙЛЫ:
        итог.add(файл)
    for каталог in КАТАЛОГИ:
        полный = os.path.join(КОРЕНЬ, каталог)
        if not os.path.isdir(полный):
            raise SystemExit(f"нет каталога для сверки: {каталог}")
        for корень, папки, имена in os.walk(полный):
            папки[:] = [п for п in папки if п not in ПРОПУСКАТЬ]
            for имя in имена:
                if имя.endswith(".pyc"):
                    continue
                путь = os.path.join(корень, имя)
                внутри = os.path.relpath(путь, КОРЕНЬ).replace(os.sep, "/")
                итог.add(внутри)
    return итог


def в_колесе(путь_к_колесу):
    префикс = "leksika_engine/runtime/"
    итог = set()
    with zipfile.ZipFile(путь_к_колесу) as архив:
        for имя in архив.namelist():
            if имя.startswith(префикс):
                итог.add(имя[len(префикс):])
    return итог


def main(argv):
    колесо = argv[1] if len(argv) > 1 else None
    if not колесо:
        найденные = sorted(glob.glob(os.path.join(КОРЕНЬ, "dist", "*.whl")))
        if not найденные:
            raise SystemExit("нужен путь к колесу: python3 packaging/pypi/проверить_состав.py dist/*.whl")
        колесо = найденные[-1]
    нужно = исходники()
    есть = в_колесе(колесо)
    нет = sorted(нужно - есть)
    if нет:
        print(f"В колесе {os.path.basename(колесо)} не хватает файлов: {len(нет)}", file=sys.stderr)
        for имя in нет[:20]:
            print(f"  нет: {имя}", file=sys.stderr)
        print("Добавьте каталог в [tool.hatch.build.targets.wheel.force-include].", file=sys.stderr)
        return 1
    лишнее = sorted(есть - нужно)
    print(f"в колесе {len(есть)} файлов движка, все {len(нужно)} нужных на месте")
    if лишнее:
        print(f"сверх исходников: {len(лишнее)} (не ошибка): {', '.join(лишнее[:5])}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
