#!/usr/bin/env python3
"""Готовит версию пакета для публикации на PyPI.

Принимает тег выпуска (``v0.4.0``, ``v0.4.0-beta.1``, ``v0.4.0-rc.2``) или уже
нормализованную версию PEP 440 (``0.4.0b1``) и:

* проверяет формат, чтобы PyPI не отклонил файл уже после сборки;
* печатает ``version=``, ``tag=``, ``prerelease=`` в формате $GITHUB_OUTPUT;
* переписывает ``leksika_engine/__about__.py``, если не указан ``--dry-run``.

Примеры:

    python3 packaging/pypi/prepare_version.py v0.4.0-beta.1 --dry-run
    python3 packaging/pypi/prepare_version.py 0.4.0b1

Соответствие: ``X.Y.Z-alpha.N`` → ``X.Y.ZaN``, ``-beta.N`` → ``X.Y.ZbN``,
``-rc.N`` → ``X.Y.ZrcN``. Другие суффиксы не поддерживаются: изменяемая
сборка на PyPI запрещена, а произвольные строки ломают сортировку версий.
"""

import argparse
import os
import re
import sys

ТЕГ = re.compile(
    r"v(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)"
    r"(?:-(?P<stage>alpha|beta|rc)\.(?P<num>0|[1-9]\d*))?\Z"
)
ВЕРСИЯ = re.compile(
    r"(?P<major>0|[1-9]\d*)\.(?P<minor>0|[1-9]\d*)\.(?P<patch>0|[1-9]\d*)"
    r"(?:(?P<stage>a|b|rc)(?P<num>0|[1-9]\d*))?\Z"
)
СТУПЕНИ = {"alpha": "a", "beta": "b", "rc": "rc"}
СЛОВА = {"a": "alpha", "b": "beta", "rc": "rc"}

КОРЕНЬ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ФАЙЛ_ВЕРСИИ = os.path.join(КОРЕНЬ, "leksika_engine", "__about__.py")

ПОДСКАЗКА = (
    "нужен X.Y.Z, X.Y.Z-alpha.N, X.Y.Z-beta.N или X.Y.Z-rc.N "
    "с тегом v впереди или без него (без ведущих нулей)"
)


def разобрать(значение):
    """Возвращает (версия PEP 440, тег, признак предварительной версии)."""
    значение = (значение or "").strip()
    if not значение:
        raise SystemExit(ПОДСКАЗКА)

    тег = значение if значение.startswith("v") else "v" + значение
    совпадение = ТЕГ.match(тег)
    if совпадение:
        мажор, минор, патч = совпадение.group("major", "minor", "patch")
        ступень, номер = совпадение.group("stage"), совпадение.group("num")
        if ступень:
            версия = f"{мажор}.{минор}.{патч}{СТУПЕНИ[ступень]}{номер}"
        else:
            версия = f"{мажор}.{минор}.{патч}"
        return версия, тег, ступень is not None

    совпадение = ВЕРСИЯ.match(значение)
    if совпадение:
        мажор, минор, патч = совпадение.group("major", "minor", "patch")
        ступень, номер = совпадение.group("stage"), совпадение.group("num")
        if ступень:
            тег = f"v{мажор}.{минор}.{патч}-{СЛОВА[ступень]}.{номер}"
        else:
            тег = f"v{мажор}.{минор}.{патч}"
        return значение, тег, ступень is not None

    raise SystemExit("неподдерживаемая версия «" + значение + "»: " + ПОДСКАЗКА)


def записать_версию(версия):
    with open(ФАЙЛ_ВЕРСИИ, encoding="utf-8") as файл:
        текст = файл.read()
    новый, сколько = re.subn(
        r'(?m)^__version__ = "[^"]*"$', f'__version__ = "{версия}"', текст
    )
    if сколько != 1:
        raise SystemExit("в leksika_engine/__about__.py нет строки __version__")
    with open(ФАЙЛ_ВЕРСИИ, "w", encoding="utf-8") as файл:
        файл.write(новый)


def main(argv=None):
    разборщик = argparse.ArgumentParser(description="Версия пакета для PyPI")
    разборщик.add_argument("версия", help="тег выпуска или версия PEP 440")
    разборщик.add_argument(
        "--dry-run",
        action="store_true",
        help="только проверить и напечатать, файл версии не менять",
    )
    аргументы = разборщик.parse_args(argv)

    версия, тег, предварительная = разобрать(аргументы.версия)
    if not аргументы.dry_run:
        записать_версию(версия)

    print(f"version={версия}")
    print(f"tag={тег}")
    print(f"prerelease={'true' if предварительная else 'false'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
