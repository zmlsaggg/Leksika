#!/usr/bin/env bash
# Запуск собранного APK в эмуляторе и проверка, что песочница посчитала программу.
# Интерфейс WebView появляется в дереве доступности не сразу, а эмулятор в CI
# иногда отвечает медленнее обычного, поэтому проверка делается с повторами.
set -Eeuo pipefail
trap 'echo "::error::APK smoke: $BASH_COMMAND (строка $LINENO)"; adb logcat -d -t 300 | tail -120' ERR

PACKAGE=org.leksika.sandbox
ACTIVITY=$PACKAGE/.MainActivity

dump_ui() {
  adb shell uiautomator dump /sdcard/window.xml >/dev/null 2>&1 || true
  adb exec-out cat /sdcard/window.xml > /tmp/leksika-android-ui.xml 2>/dev/null || true
}

wait_for_text() {
  local text="$1" tries="$2"
  for _ in $(seq 1 "$tries"); do
    dump_ui
    if grep -q "$text" /tmp/leksika-android-ui.xml 2>/dev/null; then return 0; fi
    sleep 2
  done
  return 1
}

tap_launch() {
  python3 - <<'PY'
import re, subprocess, xml.etree.ElementTree as ET
root = ET.parse('/tmp/leksika-android-ui.xml')
for node in root.iter('node'):
    if 'Запустить' in (node.get('text', ''), node.get('content-desc', '')):
        x1, y1, x2, y2 = map(int, re.findall(r'\d+', node.get('bounds', '')))
        subprocess.run(['adb', 'shell', 'input', 'tap',
                        str((x1 + x2) // 2), str((y1 + y2) // 2)], check=True)
        break
else:
    raise SystemExit('Кнопка запуска в WebView не найдена')
PY
}

attempt() {
  adb shell am force-stop "$PACKAGE" >/dev/null 2>&1 || true
  adb install -r release/01-Leksika-Android.apk
  adb shell am start -W -n "$ACTIVITY"
  # Ждём кнопку запуска: локальный редактор грузится из ресурсов APK.
  if ! wait_for_text 'Запустить' 30; then
    return 1
  fi
  tap_launch
  # Ждём результат программы: песочница печатает 17.
  for _ in $(seq 1 25); do
    dump_ui
    if grep -q 'Выполнено' /tmp/leksika-android-ui.xml 2>/dev/null \
       && grep -q '17' /tmp/leksika-android-ui.xml 2>/dev/null; then
      echo 'APK запустил программу и вывел 17.'
      return 0
    fi
    sleep 2
  done
  return 1
}

for try in 1 2; do
  if attempt; then
    exit 0
  fi
  echo "::warning::попытка $try не удалась, повторяем запуск APK"
done

cat /tmp/leksika-android-ui.xml 2>/dev/null || true
echo 'Результат программы в WebView не найден' >&2
exit 1
