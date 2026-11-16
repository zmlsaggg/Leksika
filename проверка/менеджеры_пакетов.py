#!/usr/bin/env python3
"""Проверка packaging/update_package_managers.py без сети и без секретов.

Скрипт запускается в CI рядом с «Проверить шаблоны пакетов». Он собирает
временные копии внешних репозиториев в их реальной форме (формула Homebrew
и манифест Scoop, как они опубликованы), прогоняет обновление и требует:

1. формула и манифест получили нужную версию, ссылки на тег и SHA-256;
2. остальная структура файлов не изменилась — дифф ровно в нужных строках;
3. повторный запуск идемпотентен, сухой прогон ничего не пишет;
4. ошибочные входы (предварительная версия, неполные суммы, мусор, отсутствие
   клона) завершаются кодом 1 и не портят файлы.

Проверка намеренно не ходит в сеть: суммы берутся из локального файла,
поведение скачивания проверяется подменой адреса на локальный HTTP-сервер
только в тесте режима --download.
"""

from __future__ import annotations

import contextlib
import http.server
import importlib.util
import io
import json
import shutil
import socket
import sys
import tempfile
import threading
import unittest
import urllib.parse
from pathlib import Path

КОРЕНЬ = Path(__file__).resolve().parent.parent
СКРИПТ = КОРЕНЬ / "packaging" / "update_package_managers.py"

# Формула в том виде, в каком она лежит в zmlsaggg/homebrew-leksika.
ФОРМУЛА = '''class Leksika < Formula
  desc "Экспериментальное нативное ядро русского языка программирования Лексика"
  homepage "https://leksika.gleeze.com/"
  # Текущий опубликованный комплект доступен под тегом nightly.
  version "0.4.0-beta.1"
  license "MIT"

  on_macos do
    if Hardware::CPU.arm?
      url "https://github.com/zmlsaggg/Leksika/releases/download/nightly/leksika-native-macos-arm64.tar.gz"
      sha256 "1b9efdaf92ca4f1edd9674fada6af9fc64287a2a43ec459c4989a839b8ace08a"
    else
      url "https://github.com/zmlsaggg/Leksika/releases/download/nightly/leksika-native-macos-x64.tar.gz"
      sha256 "acef63702f3eeb1b32d41ec8cec85799c880ea8ceb2b4a650034738f315a3563"
    end
  end

  on_linux do
    on_intel do
      url "https://github.com/zmlsaggg/Leksika/releases/download/nightly/leksika-native-linux-x64.tar.gz"
      sha256 "a3510944ceabc4f9becc9baf0c1975a4b7cd81b83ed36e79486a6e29d86c7feb"
    end
  end

  def install
    bin.install "leksus-core" => "Leksika"
    bin.install_symlink "Leksika" => "leksika"
  end
end
'''

# Манифест в том виде, в каком он лежит в zmlsaggg/scoop-leksika:
# вложенные массивы bin записаны в одну строку — это и проверяем на сохранность.
МАНИФЕСТ = '''{
    "version": "nightly",
    "description": "Экспериментальное нативное ядро русского языка программирования Лексика и движок Лексус",
    "homepage": "https://leksika.gleeze.com/",
    "license": "MIT",
    "architecture": {
        "64bit": {
            "url": "https://github.com/zmlsaggg/Leksika/releases/download/nightly/leksika-native-windows-x64.zip",
            "hash": "2cd9792f2dc1c75b4fcf81e8fbf81e13e1fae3f3110a327a24aada238457bf88"
        }
    },
    "bin": [
        "leksus-core.exe",
        ["leksus-core.exe", "Leksika"],
        ["leksus-core.exe", "leksika"]
    ],
    "notes": [
        "Это экспериментальное нативное ядро, а не полный исполнитель Лексики."
    ],
    "autoupdate": {
        "architecture": {
            "64bit": {
                "url": "https://github.com/zmlsaggg/Leksika/releases/download/v$version/leksika-native-windows-x64.zip",
                "hash": { "url": "https://github.com/zmlsaggg/Leksika/releases/download/v$version/zz-checksums.txt" }
            }
        }
    }
}
'''

АРХИВЫ = {
    "leksika-native-macos-arm64.tar.gz": "3" * 64,
    "leksika-native-macos-x64.tar.gz": "4" * 64,
    "leksika-native-linux-x64.tar.gz": "2" * 64,
    "leksika-native-windows-x64.zip": "1" * 64,
}


def загрузить_модуль():
    """Импортирует скрипт как модуль, чтобы проверить функции напрямую."""
    спецификация = importlib.util.spec_from_file_location("update_package_managers", СКРИПТ)
    модуль = importlib.util.module_from_spec(спецификация)
    спецификация.loader.exec_module(модуль)
    return модуль


МОДУЛЬ = загрузить_модуль()


def файл_сумм(каталог: Path, архивы: dict[str, str] | None = None) -> Path:
    архивы = АРХИВЫ if архивы is None else архивы
    строки = [f"{хэш}  {имя}" for имя, хэш in архивы.items()]
    строки.insert(0, f"{'a' * 64}  00-Leksika-Windows.exe")
    строки.append(f"{'b' * 64}  01-Leksika-Android.apk")
    путь = каталог / "zz-SHA256SUMS.txt"
    путь.write_text("\n".join(строки) + "\n", encoding="utf-8")
    return путь


class ПроверкаОбновления(unittest.TestCase):
    def setUp(self):
        self.каталог = Path(tempfile.mkdtemp(prefix="leksika-upm-"))
        self.homebrew = self.каталог / "homebrew-leksika"
        self.scoop = self.каталог / "scoop-leksika"
        (self.homebrew / "Formula").mkdir(parents=True)
        (self.scoop / "bucket").mkdir(parents=True)
        (self.homebrew / "Formula" / "leksika.rb").write_text(ФОРМУЛА, encoding="utf-8")
        (self.scoop / "bucket" / "leksika.json").write_text(МАНИФЕСТ, encoding="utf-8")
        self.суммы = файл_сумм(self.каталог)

    def tearDown(self):
        shutil.rmtree(self.каталог, ignore_errors=True)

    def запустить(self, *аргументы: str) -> int:
        ошибки = io.StringIO()
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(ошибки):
            код = МОДУЛЬ.main([str(а) for а in аргументы])
        self.последний_stderr = ошибки.getvalue()
        return код

    def обновить(self, версия: str = "1.0.0", *дополнительно: str) -> int:
        return self.запустить(
            версия,
            "--sums",
            self.суммы,
            "--homebrew",
            self.homebrew,
            "--scoop",
            self.scoop,
            *дополнительно,
        )

    def текст(self, путь: Path) -> str:
        return путь.read_text(encoding="utf-8")

    def test_формула_получает_версию_ссылки_и_суммы(self):
        self.assertEqual(self.обновить("1.0.0"), 0)
        формула = self.текст(self.homebrew / "Formula" / "leksika.rb")
        self.assertIn('version "1.0.0"', формула)
        for имя, хэш in АРХИВЫ.items():
            if имя.endswith(".zip"):
                self.assertNotIn(имя, формула)  # Windows-архив в формулу не входит
                continue
            self.assertIn(f"/download/v1.0.0/{имя}", формула)
            self.assertIn(f'sha256 "{хэш}"', формула)
        # Комментарий про nightly в формуле остаётся: он объясняет историю файла.
        # Важно, что на nightly больше не указывает ни одна ссылка.
        self.assertNotIn("/download/nightly/", формула)
        self.assertNotIn("0.4.0-beta.1", формула)

    def test_формула_сохраняет_остальной_текст(self):
        self.assertEqual(self.обновить("1.0.0"), 0)
        старое = ФОРМУЛА.splitlines()
        новое = self.текст(self.homebrew / "Formula" / "leksika.rb").splitlines()
        self.assertEqual(len(старое), len(новое), "число строк изменилось")
        изменённые = [i for i, (а, б) in enumerate(zip(старое, новое)) if а != б]
        # version, три url, три sha256 — ровно семь строк.
        self.assertEqual(len(изменённые), 7, изменённые)
        # Комментарии и установочные строки не тронуты.
        self.assertIn("  def install\n".strip(), "".join(новое))
        self.assertIn('bin.install "leksus-core" => "Leksika"', "".join(новое))

    def test_манифест_обновляется_точечно(self):
        self.assertEqual(self.обновить("2.5.1"), 0)
        путь = self.scoop / "bucket" / "leksika.json"
        манифест = json.loads(self.текст(путь))
        self.assertEqual(манифест["version"], "2.5.1")
        запись = манифест["architecture"]["64bit"]
        self.assertEqual(
            запись["url"],
            "https://github.com/zmlsaggg/Leksika/releases/download/v2.5.1/leksika-native-windows-x64.zip",
        )
        self.assertEqual(запись["hash"], АРХИВЫ["leksika-native-windows-x64.zip"])
        # Одноручные массивы bin и autoupdate остались как были.
        self.assertIn('["leksus-core.exe", "Leksika"]', self.текст(путь))
        self.assertEqual(манифест["autoupdate"], json.loads(МАНИФЕСТ)["autoupdate"])
        старое = МАНИФЕСТ.splitlines()
        новое = self.текст(путь).splitlines()
        self.assertEqual(len(старое), len(новое), "число строк изменилось")
        изменённые = [i for i, (а, б) in enumerate(zip(старое, новое)) if а != б]
        self.assertEqual(len(изменённые), 3, изменённые)

    def test_повторный_запуск_ничего_не_меняет(self):
        self.assertEqual(self.обновить("1.0.0"), 0)
        формула = self.текст(self.homebrew / "Formula" / "leksika.rb")
        манифест = self.текст(self.scoop / "bucket" / "leksika.json")
        self.assertEqual(self.обновить("1.0.0"), 0)
        self.assertEqual(формула, self.текст(self.homebrew / "Formula" / "leksika.rb"))
        self.assertEqual(манифест, self.текст(self.scoop / "bucket" / "leksika.json"))

    def test_сухой_прогон_ничего_не_пишет(self):
        self.assertEqual(self.обновить("1.0.0", "--dry-run"), 0)
        self.assertEqual(ФОРМУЛА, self.текст(self.homebrew / "Formula" / "leksika.rb"))
        self.assertEqual(МАНИФЕСТ, self.текст(self.scoop / "bucket" / "leksika.json"))

    def test_предварительная_версия_отвергается(self):
        self.assertEqual(self.обновить("1.0.0-beta.1"), 1)
        self.assertEqual(ФОРМУЛА, self.текст(self.homebrew / "Formula" / "leksika.rb"))

    def test_префикс_v_отвергается(self):
        self.assertEqual(self.обновить("v1.0.0"), 1)

    def test_неполные_суммы_отвергаются(self):
        неполные = файл_сумм(self.каталог, {"leksika-native-windows-x64.zip": "1" * 64})
        код = self.запустить(
            "1.0.0", "--sums", неполные, "--homebrew", self.homebrew, "--scoop", self.scoop
        )
        self.assertEqual(код, 1)
        self.assertIn("нет сумм для архивов", self.последний_stderr)
        self.assertEqual(ФОРМУЛА, self.текст(self.homebrew / "Formula" / "leksika.rb"))

    def test_мусор_вместо_сумм_отвергается(self):
        мусор = self.каталог / "мусор.txt"
        мусор.write_text("не сумма  leksika-native-windows-x64.zip\n", encoding="utf-8")
        код = self.запустить("1.0.0", "--sums", мусор, "--scoop", self.scoop)
        self.assertEqual(код, 1)

    def test_отсутствующий_клон_объясняется(self):
        код = self.запустить(
            "1.0.0", "--sums", self.суммы, "--homebrew", self.каталог / "нет-такого"
        )
        self.assertEqual(код, 1)
        self.assertIn("сначала склонируйте репозиторий", self.последний_stderr)

    def test_выбор_источника_сумм_проверяется(self):
        self.assertEqual(self.запустить("1.0.0", "--homebrew", self.homebrew), 1)
        self.assertEqual(
            self.запустить("1.0.0", "--sums", self.суммы, "--download", "--homebrew", self.homebrew),
            1,
        )

    def test_только_один_репозиторий_обновляется_по_требованию(self):
        self.assertEqual(
            self.запустить("1.0.0", "--sums", self.суммы, "--scoop", self.scoop), 0
        )
        self.assertEqual(ФОРМУЛА, self.текст(self.homebrew / "Formula" / "leksika.rb"))
        self.assertIn('"version": "1.0.0"', self.текст(self.scoop / "bucket" / "leksika.json"))

    def test_формула_с_недостатком_sha_отвергается(self):
        путь = self.homebrew / "Formula" / "leksika.rb"
        путь.write_text(
            ФОРМУЛА.replace(
                '      sha256 "a3510944ceabc4f9becc9baf0c1975a4b7cd81b83ed36e79486a6e29d86c7feb"\n', ""
            ),
            encoding="utf-8",
        )
        код = self.запустить(
            "1.0.0", "--sums", self.суммы, "--homebrew", self.homebrew
        )
        self.assertEqual(код, 1)
        self.assertIn("нет суммы для архивов", self.последний_stderr)
        self.assertIn("leksika-native-linux-x64.tar.gz", self.последний_stderr)

    def test_переставленные_блоки_не_путают_суммы(self):
        # Linux перед macOS: сумма должна идти за своим url, а не за порядком.
        путь = self.homebrew / "Formula" / "leksika.rb"
        linux_блок = (
            '  on_linux do\n'
            '    on_intel do\n'
            '      url "https://github.com/zmlsaggg/Leksika/releases/download/nightly/leksika-native-linux-x64.tar.gz"\n'
            '      sha256 "a3510944ceabc4f9becc9baf0c1975a4b7cd81b83ed36e79486a6e29d86c7feb"\n'
            '    end\n'
            '  end\n'
        )
        переставлено = ФОРМУЛА.replace(
            linux_блок, ""
        ).replace("  on_macos do", linux_блок + "\n  on_macos do")
        self.assertIn("leksika-native-linux-x64", переставлено)
        self.assertLess(
            переставлено.index("linux-x64"), переставлено.index("macos-arm64"),
            "блок Linux должен оказаться раньше macOS",
        )
        путь.write_text(переставлено, encoding="utf-8")
        self.assertEqual(
            self.запустить("1.0.0", "--sums", self.суммы, "--homebrew", self.homebrew), 0
        )
        итог = self.текст(путь)
        # Каждая сумма стоит рядом со своим архивом.
        строки = итог.splitlines()
        for платформа in ("linux-x64", "macos-arm64", "macos-x64"):
            архив = f"leksika-native-{платформа}.tar.gz"
            индекс = next(i for i, с in enumerate(строки) if архив in с and "url" in с)
            self.assertEqual(
                строки[индекс + 1].strip(), f'sha256 "{АРХИВЫ[архив]}"', архив
            )

    def test_сумма_без_архива_отвергается(self):
        путь = self.homebrew / "Formula" / "leksika.rb"
        путь.write_text(
            ФОРМУЛА.replace(
                '      url "https://github.com/zmlsaggg/Leksika/releases/download/nightly/leksika-native-linux-x64.tar.gz"\n',
                "",
            ),
            encoding="utf-8",
        )
        код = self.запустить("1.0.0", "--sums", self.суммы, "--homebrew", self.homebrew)
        self.assertEqual(код, 1)
        self.assertIn("раньше url", self.последний_stderr)

    def test_манифест_без_64bit_отвергается(self):
        путь = self.scoop / "bucket" / "leksika.json"
        манифест = json.loads(МАНИФЕСТ)
        del манифест["architecture"]["64bit"]
        путь.write_text(json.dumps(манифест, ensure_ascii=False), encoding="utf-8")
        код = self.запустить("1.0.0", "--sums", self.суммы, "--scoop", self.scoop)
        self.assertEqual(код, 1)


class ПроверкаСкачивания(unittest.TestCase):
    """Режим --download проверяется на локальном сервере, без GitHub."""

    def setUp(self):
        self.каталог = Path(tempfile.mkdtemp(prefix="leksika-sums-"))
        self.сервер = http.server.ThreadingHTTPServer(("127.0.0.1", 0), self.обработчик())
        self.порт = self.сервер.server_address[1]
        threading.Thread(target=self.сервер.serve_forever, daemon=True).start()

    def обработчик(self):
        каталог = self.каталог

        class Обработчик(http.server.BaseHTTPRequestHandler):
            def do_GET(self):  # noqa: N802 — имя задано базовым классом
                имя = Path(urllib.parse.unquote(self.path)).name
                файл = каталог / "отдать" / имя
                if not файл.is_file():
                    self.send_error(404)
                    return
                данные = файл.read_bytes()
                self.send_response(200)
                self.send_header("Content-Length", str(len(данные)))
                self.end_headers()
                self.wfile.write(данные)

            def log_message(self, *аргументы):
                pass

        return Обработчик

    def tearDown(self):
        self.сервер.shutdown()
        self.сервер.server_close()
        shutil.rmtree(self.каталог, ignore_errors=True)

    def test_скачанные_суммы_разбираются(self):
        (self.каталог / "отдать").mkdir()
        файл_сумм(self.каталог / "отдать")
        адрес = f"http://127.0.0.1:{self.порт}/zz-SHA256SUMS.txt"
        куда = self.каталог / "скачанное" / "суммы.txt"
        with contextlib.redirect_stdout(io.StringIO()):
            # Подменяем только адрес: механизм скачивания и разбора — настоящий.
            исходный, МОДУЛЬ.адрес_релиза = МОДУЛЬ.адрес_релиза, lambda версия, архив: адрес
            try:
                путь = МОДУЛЬ.скачать_суммы("1.0.0", куда)
            finally:
                МОДУЛЬ.адрес_релиза = исходный
        self.assertEqual(путь.read_bytes().count(b"\n"), 6)
        суммы = МОДУЛЬ.прочитать_суммы(путь)
        self.assertEqual(суммы["leksika-native-windows-x64.zip"], "1" * 64)

    def test_404_объясняется(self):
        адрес = f"http://127.0.0.1:{self.порт}/missing.txt"
        исходный, МОДУЛЬ.адрес_релиза = МОДУЛЬ.адрес_релиза, lambda версия, архив: адрес
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(МОДУЛЬ.ОшибкаПроверки) as контекст:
                    МОДУЛЬ.скачать_суммы("1.0.0", self.каталог / "суммы.txt")
        finally:
            МОДУЛЬ.адрес_релиза = исходный
        self.assertIn("404", str(контекст.exception))

    def test_недоступный_адрес_объясняется(self):
        # Порт занят и сразу закрыт: соединение отклоняется, как при отсутствии сети.
        with socket.socket() as сокет:
            сокет.bind(("127.0.0.1", 0))
            свободный = сокет.getsockname()[1]
        адрес = f"http://127.0.0.1:{свободный}/zz-SHA256SUMS.txt"
        исходный, МОДУЛЬ.адрес_релиза = МОДУЛЬ.адрес_релиза, lambda версия, архив: адрес
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                with self.assertRaises(МОДУЛЬ.ОшибкаПроверки) as контекст:
                    МОДУЛЬ.скачать_суммы("1.0.0", self.каталог / "суммы.txt")
        finally:
            МОДУЛЬ.адрес_релиза = исходный
        self.assertIn("нет сети", str(контекст.exception))


if __name__ == "__main__":
    unittest.main(verbosity=2)
