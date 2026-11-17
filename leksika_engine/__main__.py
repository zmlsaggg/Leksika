"""Запуск без команды в PATH: ``python -m leksika_engine справка``.

В Windows каталог со скриптами pip часто не входит в PATH, и имя ``leksika``
не находится. Запуск через ``-m`` работает всегда: он идёт от того же
интерпретатора, которым поставлен пакет.
"""

import sys

from .cli import main

if __name__ == "__main__":
    sys.exit(main())
