#!/usr/bin/env python3
"""Отправка выпущенных файлов в VirusTotal и ссылки в описание релиза.

Зачем своё, а не готовый экшен: у проекта правило — меньше зависимости от
чужого кода в цепочке выпуска. Здесь только стандартная библиотека и
публичный HTTP API VirusTotal версии 3.

Бесплатный ключ даёт 4 запроса в минуту и 500 в сутки на файл. Скрипт
строго выдерживает этот темп: отправляет по одному файлу, ждёт между ними
и делает не больше двух попыток на файл.

Если ключа нет, скрипт ничего не делает и выходит с кодом 0: проверка
полезна, но её отсутствие не должно срывать выпуск.

  python3 проверка/virustotal.py отправка  --каталог release \
      --описание release/zz-SHA256SUMS.txt [--добавить-в-описание]
  python3 проверка/virustotal.py самотест

Режим «самотест» проверяет разбор ответов и вставку блока без сети.
"""

import argparse
import hashlib
import json
import os
import re
import sys
import time
import urllib.error
import urllib.request

АДРЕС_API = "https://www.virustotal.com/api/v3"
ЗАГОЛОВОК_БЛОКА = "### Проверка безопасности (VirusTotal)"
# Свои файлы отправляем не чаще, чем раз в 16 секунд: лимит бесплатного
# ключа — 4 запроса в минуту, и упереться в 429 хуже, чем подождать.
ПАУЗА_СЕКУНД = 16.0
РАСШИРЕНИЯ = (".exe", ".zip", ".tar.gz", ".apk")
КРУПНЫЙ_ФАЙЛ = 32 * 1024 * 1024  # больше этого API принимает только по ссылке


class ОшибкаVirusTotal(Exception):
    pass


def выбрать_файлы(каталог):
    """Только двоичные сборки: суммы и описания проверять незачем."""
    файлы = []
    for имя in sorted(os.listdir(каталог)):
        путь = os.path.join(каталог, имя)
        if not os.path.isfile(путь) or not имя.endswith(РАСШИРЕНИЯ):
            continue
        файлы.append(путь)
    return файлы


def _запрос(адрес, ключ, данные=None, метод=None):
    запрос = urllib.request.Request(адрес, data=данные, method=метод)
    запрос.add_header("x-apikey", ключ)
    if данные is not None:
        запрос.add_header("Content-Type", "application/x-www-form-urlencoded")
    try:
        with urllib.request.urlopen(запрос, timeout=120) as ответ:
            return json.loads(ответ.read().decode("utf-8"))
    except urllib.error.HTTPError as ошибка:
        текст = ошибка.read().decode("utf-8", "replace")[:200]
        raise ОшибкаVirusTotal(f"HTTP {ошибка.code}: {текст}") from ошибка
    except urllib.error.URLError as ошибка:
        raise ОшибкаVirusTotal(f"сеть недоступна: {ошибка.reason}") from ошибка


def отправить_файл(путь, ключ):
    """Загружает файл multipart-ом, собранным руками: без requests."""
    with open(путь, "rb") as файл:
        содержимое = файл.read()
    if len(содержимое) > КРУПНЫЙ_ФАЙЛ:
        raise ОшибкаVirusTotal(
            f"{os.path.basename(путь)} больше 32 МиБ: API принимает такой файл "
            "только по ссылке, а публичная ссылка появляется лишь после выпуска"
        )
    граница = "----лексика" + hashlib.sha256(содержимое).hexdigest()[:16]
    имя = os.path.basename(путь).encode("utf-8")
    тело = (
        b"--" + граница.encode() + b"\r\n"
        b'Content-Disposition: form-data; name="file"; filename="' + имя + b'"\r\n'
        b"Content-Type: application/octet-stream\r\n\r\n"
        + содержимое + b"\r\n"
        b"--" + граница.encode() + b"--\r\n"
    )
    запрос = urllib.request.Request(АДРЕС_API + "/files", data=тело, method="POST")
    запрос.add_header("x-apikey", ключ)
    запрос.add_header("Content-Type", f"multipart/form-data; boundary={граница}")
    try:
        with urllib.request.urlopen(запрос, timeout=300) as ответ:
            данные = json.loads(ответ.read().decode("utf-8"))
    except urllib.error.HTTPError as ошибка:
        текст = ошибка.read().decode("utf-8", "replace")[:200]
        raise ОшибкаVirusTotal(f"HTTP {ошибка.code}: {текст}") from ошибка
    except urllib.error.URLError as ошибка:
        raise ОшибкаVirusTotal(f"сеть недоступна: {ошибка.reason}") from ошибка
    анализ = данные.get("data", {}).get("id")
    if not анализ:
        raise ОшибкаVirusTotal("ответ без идентификатора анализа")
    return анализ


def ссылка_на_отчёт(анализ):
    return f"https://www.virustotal.com/gui/file-analysis/{анализ}"


def блок_описания(пары):
    """Сворачиваемый блок со ссылками: пары «файл=ссылка»."""
    строки = ["", ЗАГОЛОВОК_БЛОКА, "<details>",
              "<summary>Отчёты антивирусной проверки файлов</summary>", ""]
    for путь, ссылка in пары:
        строки.append(f"- [{os.path.basename(путь)}]({ссылка})")
    строки += ["</details>", ""]
    return "\n".join(строки)


def нужен_блок(описание):
    """Повторный запуск не должен добавлять блок второй раз."""
    return ЗАГОЛОВОК_БЛОКА not in описание


def разобрать_пары(текст):
    """«файл=ссылка» через запятую, как приходит из шага отправки."""
    пары = []
    for кусок in (текст or "").split(","):
        кусок = кусок.strip()
        if not кусок or "=" not in кусок:
            continue
        путь, ссылка = кусок.split("=", 1)
        пары.append((путь.strip(), ссылка.strip()))
    return пары


def отправить_все(файлы, ключ, лог=print, пауза=time.sleep):
    """По одному файлу с паузой; одиночная неудача не рушит релиз."""
    пары = []
    for место, путь in enumerate(файлы):
        if место:
            пауза(ПАУЗА_СЕКУНД)
        try:
            анализ = отправить_файл(путь, ключ)
        except ОшибкаVirusTotal as ошибка:
            лог(f"::warning::VirusTotal не принял {os.path.basename(путь)}: {ошибка}")
            continue
        лог(f"VirusTotal принял {os.path.basename(путь)}")
        пары.append((путь, ссылка_на_отчёт(анализ)))
    return пары


def самотест():
    проверки = []

    def проверить(имя, значение):
        проверки.append((имя, bool(значение)))

    блок = блок_описания([("release/00-Leksika-Windows.exe", "https://vt/a"),
                          ("release/leksika-native-linux-x64.tar.gz", "https://vt/b")])
    проверить("блок начинается с пустой строки", блок.startswith("\n### Проверка"))
    проверить("блок сворачиваемый", "<details>" in блок and "</details>" in блок)
    проверить("в блоке имя файла Windows", "00-Leksika-Windows.exe" in блок)
    проверить("в блоке архивы Linux", "leksika-native-linux-x64.tar.gz" in блок)
    проверить("нет повтора блока", not нужен_блок(блок))
    проверить("в пустом описании блок нужен", нужен_блок("# Leksika v1.0.0"))
    проверить("блок в середине описания не дублируется",
              not нужен_блок("# Заголовок\n\n" + блок + "\nхвост"))

    пары = разобрать_пары("a.exe=https://vt/x,b.zip=https://vt/y")
    проверить("пары разбираются", пары == [("a.exe", "https://vt/x"), ("b.zip", "https://vt/y")])
    проверить("пустой текст даёт пусто", разобрать_пары("") == [])
    проверить("мусор пропускается", разобрать_пары("без равно") == [])
    проверить("ссылка ведёт на отчёт", "file-analysis/" in ссылка_на_отчёт("ид"))

    # Отправка не должна падать на первом отказе и обязана выдерживать паузу.
    # Подменяем функцию в модуле: имя читается при вызове, поэтому этого
    # достаточно, и в сеть самотест не ходит.
    паузы, сообщения = [], []
    старый = globals()["отправить_файл"]
    try:
        globals()["отправить_файл"] = _подделка(успех={"a.exe": "ид1"}, отказ={"b.zip": "429"})
        результат = отправить_все(["release/a.exe", "release/b.zip", "release/c.exe"],
                                 ключ="ключ", лог=сообщения.append, пауза=паузы.append)
    finally:
        globals()["отправить_файл"] = старый
    # Из трёх файлов b.zip отвергнут, значит приняты два — и порядок сохранён.
    проверить("отказ одного файла не останавливает остальные", len(результат) == 2)
    проверить("принятые файлы сохраняют порядок",
              [os.path.basename(путь) for путь, _ in результат] == ["a.exe", "c.exe"])
    проверить("между файлами есть пауза", паузы == [ПАУЗА_СЕКУНД, ПАУЗА_СЕКУНД])
    проверить("отказ попал в предупреждения", any("::warning::" in м for м in сообщения))

    провалов = 0
    for имя, годно in проверки:
        print(("  верно   " if годно else "  НЕВЕРНО ") + имя)
        провалов += 0 if годно else 1
    print(f"\nпроверено: {len(проверки)}, провалено: {провалов}")
    return 0 if провалов == 0 else 1


def _подделка(успех, отказ):
    """Подменяет сетевую отправку для самотеста без обращения к API."""
    def отправка(путь, ключ):
        имя = os.path.basename(путь)
        if имя in отказ:
            raise ОшибкаVirusTotal(отказ[имя])
        return успех.get(имя, "ид")
    return отправка


def main(argv):
    разбор = argparse.ArgumentParser(description="Проверка выпуска в VirusTotal")
    подкоманды = разбор.add_subparsers(dest="дело", required=True)

    отправка = подкоманды.add_parser("отправка", help="отправить файлы выпуска")
    отправка.add_argument("--каталог", default="release")
    отправка.add_argument("--добавить-в-описание", action="store_true",
                          help="дописать блок ссылок в описание выпуска")
    отправка.add_argument("--описание", help="файл с текстом релиза")

    подкоманды.add_parser("самотест", help="проверить разбор без сети")

    аргументы = разбор.parse_args(argv)

    if аргументы.дело == "самотест":
        return самотест()

    ключ = os.environ.get("VT_API_KEY", "").strip()
    if not ключ:
        print("VirusTotal пропущен: нет ключа VT_API_KEY. Выпуск не затронут.")
        return 0

    файлы = выбрать_файлы(аргументы.каталог)
    if not файлы:
        print(f"В {аргументы.каталог} нет файлов для проверки.")
        return 0

    пары = отправить_все(файлы, ключ)
    print("Отправлено файлов: %d из %d" % (len(пары), len(файлы)))
    if not пары:
        print("::warning::VirusTotal не принял ни одного файла, блок в описании не добавлен")
        return 0

    if аргументы.добавить_в_описание:
        if not аргументы.описание:
            print("Не указан файл описания: --описание", file=sys.stderr)
            return 2
        with open(аргументы.описание, encoding="utf-8") as файл:
            описание = файл.read()
        if not нужен_блок(описание):
            print("Блок VirusTotal уже есть в описании, повтор не нужен.")
            return 0
        with open(аргументы.описание, "w", encoding="utf-8") as файл:
            файл.write(описание.rstrip("\n") + "\n" + блок_описания(пары))
        print("Блок со ссылками дописан в описание.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
