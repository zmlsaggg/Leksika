"""Запускатор движка Лексики для установки из PyPI.

Файлы .lek — это исходники Python с русскими именами, поэтому пакет не
подменяет импорт, а передаёт управление штатному входу движка
(``лексус.lek``) и его загрузчику ``.lek``-частей.
"""

import os
import runpy
import sys

ИЗДАНИЕ = os.path.dirname(os.path.abspath(__file__))
ДВИЖОК = os.path.join(ИЗДАНИЕ, "runtime")
ВХОД = os.path.join(ДВИЖОК, "лексус.lek")


def _настроить_вывод():
    """UTF-8 в stdout/stderr: на Windows и в старых терминалах кодировка иная."""
    for поток in (sys.stdout, sys.stderr):
        настроить = getattr(поток, "reconfigure", None)
        if настроить is None:
            continue
        try:
            настроить(encoding="utf-8")
        except (OSError, ValueError):
            pass


def main(argv=None):
    if not os.path.isfile(ВХОД):
        print(
            "Лексика: в пакете нет движка (ожидался runtime/лексус.lek). "
            "Переустановите пакет: pip install --force-reinstall leksika",
            file=sys.stderr,
        )
        return 2
    if sys.version_info < (3, 8):
        print("Лексика: нужен Python 3.8 или новее", file=sys.stderr)
        return 2

    _настроить_вывод()
    os.environ.setdefault("PYTHONUTF8", "1")
    os.environ.setdefault("PYTHONIOENCODING", "utf-8")

    # Движок ищет части «лексика» по sys.path относительно собственного файла.
    if ДВИЖОК not in sys.path:
        sys.path.insert(0, ДВИЖОК)

    sys.argv = [ВХОД] + list(sys.argv[1:] if argv is None else argv)
    runpy.run_path(ВХОД, run_name="__main__")
    return 0
