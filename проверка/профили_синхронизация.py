#!/usr/bin/env python3
"""Проверяет, что native C и браузер используют один набор синонимов профилей."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROFILE_FILE = ROOT / "данные" / "словарь" / "разговорные_слова.json"
NATIVE_FILE = ROOT / "ядро" / "компилятор.c"
BROWSER_FILE = ROOT / "playground.js"

FLAGS = {
    "взрослая_лексика": "ЛЕКСУС_ПРОФИЛЬ_18_ПЛЮС",
    "зумер_лексика": "ЛЕКСУС_ПРОФИЛЬ_ЗУМЕР",
}
SUPPORTED = {
    "сказать", "увеличить", "уменьшить", "вернуть", "прервать", "продолжить"
}

profiles = json.loads(PROFILE_FILE.read_text(encoding="utf-8"))
native = NATIVE_FILE.read_text(encoding="utf-8")
browser = BROWSER_FILE.read_text(encoding="utf-8")

expected = set()
for profile_name, profile in profiles["profiles"].items():
    flag = FLAGS[profile_name]
    for command, forms in profile.get("command_aliases", {}).items():
        if command in SUPPORTED:
            expected.update((form, command, flag) for form in forms)

actual = set(re.findall(
    r'\{"([^"]+)","([^"]+)",([^}]+)\}', native
))
if actual != expected:
    missing = sorted(expected - actual)
    extra = sorted(actual - expected)
    raise SystemExit(f"Профили расходятся с native C. Нет: {missing}; лишнее: {extra}")

match = re.search(
    r'const поддерживаемыеПрофильныеКоманды = new Set\(\[(.*?)\]\);',
    browser, re.S
)
if not match:
    raise SystemExit("В браузере не найдена таблица поддерживаемых командных синонимов")
browser_supported = set(re.findall(r'"([^"]+)"', match.group(1)))
if browser_supported != SUPPORTED:
    raise SystemExit(
        f"Профили браузера и native используют разные команды: "
        f"browser={sorted(browser_supported)}, native={sorted(SUPPORTED)}"
    )

if 'fetch("/данные/словарь/разговорные_слова.json"' not in browser:
    raise SystemExit("Браузер больше не загружает общий JSON профилей")

print(f"Профили синхронизированы: {len(expected)} синонимов команд; {len(SUPPORTED)} команд.")
