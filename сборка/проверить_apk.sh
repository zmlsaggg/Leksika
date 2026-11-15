#!/usr/bin/env bash
set -Eeuo pipefail
trap 'echo "::error::APK smoke: $BASH_COMMAND (строка $LINENO)"; adb logcat -d -t 300 | tail -120' ERR
adb install -r release/01-Leksika-Android.apk
adb shell am start -W -n org.leksika.sandbox/.MainActivity
# Дожидаемся отображения локального редактора, не полагаясь на координаты экрана.
for attempt in {1..20}; do
  adb shell uiautomator dump /sdcard/window.xml >/dev/null
  adb exec-out cat /sdcard/window.xml > /tmp/leksika-android-ui.xml
  if grep -q 'Запустить' /tmp/leksika-android-ui.xml; then break; fi
  sleep 2
done
python3 - <<'PY'
import re, subprocess, xml.etree.ElementTree as ET
root = ET.parse('/tmp/leksika-android-ui.xml')
for n in root.iter('node'):
    if 'Запустить' in (n.get('text',''), n.get('content-desc','')):
        x1,y1,x2,y2=map(int,re.findall(r'\d+',n.get('bounds','')))
        subprocess.run(['adb','shell','input','tap',str((x1+x2)//2),str((y1+y2)//2)],check=True)
        break
else: raise SystemExit('Кнопка запуска в WebView не найдена')
PY
for attempt in {1..15}; do
  adb shell uiautomator dump /sdcard/window.xml >/dev/null
  adb exec-out cat /sdcard/window.xml > /tmp/leksika-android-ui.xml
  if grep -q 'Выполнено' /tmp/leksika-android-ui.xml && grep -q '17' /tmp/leksika-android-ui.xml; then
    echo 'APK запустил программу и вывел 17.'; exit 0
  fi
  sleep 2
done
cat /tmp/leksika-android-ui.xml
echo 'Результат программы в WebView не найден' >&2; exit 1
