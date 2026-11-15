"""Необязательная загрузка закреплённых GGUF-моделей; без установки inference runtime."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import sys
import tempfile
import time
import urllib.request
from urllib.parse import urlparse

# Ревизии, размеры и SHA-256 взяты из карточек официальных репозиториев HF.
МОДЕЛИ = {
    'gigachat': {
        'имя': 'GigaChat3.1-10B-A1.8B-q4_K_M.gguf',
        'ссылка': 'https://huggingface.co/ai-sage/GigaChat3.1-10B-A1.8B-GGUF/resolve/97045b260251cfa86f5ad25638fa2dd074153446/GigaChat3.1-10B-A1.8B-q4_K_M.gguf',
        'размер': 6474702976,
        'sha256': '68a8732fb5cee04f83ebffd7924e15c534d4442c5a43d2ba9e2041fe310b8deb',
    },
    'yandex': {
        'имя': 'YandexGPT-5-Lite-8B-instruct-Q4_K_M.gguf',
        'ссылка': 'https://huggingface.co/yandex/YandexGPT-5-Lite-8B-instruct-GGUF/resolve/9fe287d2f512503046bb008aed350f2b4bbb903d/YandexGPT-5-Lite-8B-instruct-Q4_K_M.gguf',
        'размер': 4920741184,
        'sha256': 'd9ff5b826f20fbcc2f898f9f2349ac21241579f8fbb79cd32148a333623ba228',
    },
}


class ТолькоHTTPS(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urlparse(newurl).scheme.lower() != 'https':
            raise ValueError('Перенаправление на незащищённый адрес запрещено')
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def открыть(адрес):
    if urlparse(адрес).scheme.lower() != 'https':
        raise ValueError('Нужен HTTPS-адрес')
    return urllib.request.build_opener(ТолькоHTTPS()).open(
        urllib.request.Request(адрес, headers={'User-Agent': 'Leksika-model-downloader/1'}), timeout=60)


def хеш_файла(путь):
    хеш = hashlib.sha256()
    with open(путь, 'rb') as файл:
        for блок in iter(lambda: файл.read(1024 * 1024), b''):
            хеш.update(блок)
    return хеш.hexdigest()


def скачать(модель, папка, открыть_поток=открыть, вывод=print):
    папка = Path(папка)
    папка.mkdir(parents=True, exist_ok=True)
    цель = папка / модель['имя']
    размер = модель['размер']
    if цель.is_file() and цель.stat().st_size == размер and хеш_файла(цель) == модель['sha256']:
        вывод(f'Уже загружена и проверена: {цель}')
        return цель
    if shutil.disk_usage(папка).free < размер:
        raise OSError(f'Недостаточно места для {модель["имя"]}: требуется {размер} байт')
    временный = None
    try:
        with tempfile.NamedTemporaryFile(prefix='.leksika-', suffix='.part', dir=папка, delete=False) as файл:
            временный = Path(файл.name)
            хеш = hashlib.sha256()
            получено = 0
            начало = последнее = time.monotonic()
            вывод(f'Загрузка: {модель["имя"]} ({размер / 1e9:.2f} ГБ)')
            with открыть_поток(модель['ссылка']) as ответ:
                while True:
                    блок = ответ.read(1024 * 1024)
                    if not блок:
                        break
                    получено += len(блок)
                    if получено > размер:
                        raise ValueError('Полученный файл больше ожидаемого')
                    файл.write(блок)
                    хеш.update(блок)
                    сейчас = time.monotonic()
                    if сейчас - последнее >= 1:
                        скорость = получено / max(сейчас - начало, 0.001)
                        секунд = (размер - получено) / скорость
                        вывод(f'{получено * 100 / размер:.1f}% · осталось примерно {секунд / 60:.1f} мин')
                        последнее = сейчас
            if получено != размер or хеш.hexdigest() != модель['sha256']:
                raise ValueError('Размер или SHA-256 модели не совпадает; файл не установлен')
        os.replace(временный, цель)
        временный = None
        вывод(f'Модель проверена и сохранена: {цель}')
        return цель
    finally:
        if временный is not None:
            временный.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser(description='Загрузить выбранные локальные модели Лексики')
    parser.add_argument('модели', nargs='+', choices=tuple(МОДЕЛИ))
    parser.add_argument('--окно', action='store_true', help='не закрывать окно после завершения')
    args = parser.parse_args()
    корень = os.environ.get('LOCALAPPDATA') or str(Path.home())
    папка = Path(корень) / 'Лексика-модели'
    код = 0
    try:
        for имя in dict.fromkeys(args.модели):
            try:
                путь = скачать(МОДЕЛИ[имя], папка)
                print(f'Для подключения: Leksika настройка путь_к_модели "{путь}"')
            except (OSError, ValueError) as ошибка:
                print(f'Не удалось скачать {имя}: {ошибка}', file=sys.stderr)
                код = 1
        print('Для запуска модели нужен совместимый llama-cli; он не устанавливается автоматически.')
        print('Выберите одну модель: Leksika настройка умная_модель внешняя')
    except KeyboardInterrupt:
        print('\nЗагрузка прервана; временный файл удалён.')
        код = 1
    if args.окно:
        try:
            input('\nНажмите Enter, чтобы закрыть окно...')
        except (EOFError, KeyboardInterrupt):
            pass
    return код


if __name__ == '__main__':
    sys.exit(main())
