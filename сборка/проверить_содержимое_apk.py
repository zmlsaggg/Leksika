#!/usr/bin/env python3
"""Проверяет, что в APK лежат словарь форм и нативные ядра под нужные ABI.

Запускается из сборка/собрать_apk.sh и отдельно: python3 сборка/проверить_содержимое_apk.py <apk>.
Список архитектур берётся из LEKSUS_CORES в том же виде, в каком его получает
сборка («arm64-v8a=путь x86_64=путь»); проверяются только имена архитектур.
"""
import os
import sys
import zipfile

ELF_MAGIC = b"\x7fELF"


def проверить(путь_apk: str, архитектуры: list) -> list:
    ошибки = []
    with zipfile.ZipFile(путь_apk) as архив:
        записи = {сведения.filename: сведения for сведения in архив.infolist()}
        имена = list(записи)
        if "assets/dictionary.lxd.gz" not in записи:
            ошибки.append("в APK нет словаря форм assets/dictionary.lxd.gz")
        else:
            размер = записи["assets/dictionary.lxd.gz"].file_size
            if размер < 1024:
                ошибки.append("словарь форм в APK подозрительно мал: %d байт" % размер)
        for abi in архитектуры:
            путь = "lib/%s/libleksus-core.so" % abi
            if путь not in записи:
                ошибки.append("в APK нет нативного ядра %s" % путь)
                continue
            if записи[путь].compress_type != zipfile.ZIP_STORED:
                ошибки.append("ядро %s лежит в APK сжатым: запустить его нельзя" % путь)
                continue
            if архив.read(путь)[:4] != ELF_MAGIC:
                ошибки.append("%s не ELF: ядро собрано не под Android" % путь)
        if ошибки:
            print("файлы APK:", ", ".join(имена[:30]))
    return ошибки


def main(argv: list) -> int:
    if len(argv) < 2:
        print("укажите путь к APK", file=sys.stderr)
        return 2
    архитектуры = []
    for пара in os.environ.get("LEKSUS_CORES", "").split():
        abi = пара.split("=", 1)[0]
        if abi and abi not in архитектуры:
            архитектуры.append(abi)
    ошибки = проверить(argv[1], архитектуры)
    if ошибки:
        for ошибка in ошибки:
            print("::error::" + ошибка)
        return 1
    for abi in архитектуры:
        print("нативное ядро внутри APK: %s, ELF на месте" % abi)
    print("словарь форм внутри APK на месте")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
