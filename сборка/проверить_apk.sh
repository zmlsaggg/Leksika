#!/usr/bin/env bash
set -Eeuo pipefail
trap 'echo "::error::APK smoke: $BASH_COMMAND (строка $LINENO), код возврата $?"; adb logcat -d -t 300 | tail -120' ERR

# Эмулятор в общих раннерах иногда отвечает медленнее обычного, поэтому вся
# проверка повторяется один раз: холодный запуск после загрузки системы дешевле,
# чем разбираться с ложным красным прогоном.
attempt() (
  set -Eeuo pipefail
  trap 'echo "::error::APK smoke: $BASH_COMMAND (строка $LINENO), код возврата $?"; adb logcat -d -t 300 | tail -120' ERR
  adb shell am force-stop org.leksika.sandbox >/dev/null 2>&1 || true
  adb install -r release/01-Leksika-Android.apk
  # Флаг «автозапуск» включает в приложении закрытый мост: приложение само
  # нажимает «Запустить». Искать кнопку по тексту на экране ненадёжно — размер
  # экрана, разметка и прокрутка меняются, а проверять надо работу машины.
  adb shell am start -W -n org.leksika.sandbox/.MainActivity --ez автозапуск true

  # Запасной путь: если кнопка всё-таки видна в разметке, нажимаем её по
  # координатам. Ничего не найдено — не ошибка, работает мост автозапуска.
  for attempt_tap in {1..10}; do
    adb shell uiautomator dump /sdcard/window.xml >/dev/null 2>&1 || true
    if adb exec-out cat /sdcard/window.xml > /tmp/leksika-android-ui.xml 2>/dev/null &&
       grep -q 'Запустить' /tmp/leksika-android-ui.xml; then
      python3 - <<'PY'
import re, subprocess, xml.etree.ElementTree as ET
root = ET.parse('/tmp/leksika-android-ui.xml')
for n in root.iter('node'):
    if 'Запустить' in (n.get('text',''), n.get('content-desc','')):
        x1,y1,x2,y2=map(int,re.findall(r'\d+',n.get('bounds','')))
        subprocess.run(['adb','shell','input','tap',str((x1+x2)//2),str((y1+y2)//2)],check=False)
        print('кнопка нажата по координатам')
        break
PY
      break
    fi
    sleep 2
  done

  # Приложение уже нажало кнопку само; ждём, пока в разметке появится
  # результат — «Выполнено» и число 17 из первого примера.
  for attempt_wait in {1..25}; do
    adb shell uiautomator dump /sdcard/window.xml >/dev/null 2>&1 || true
    adb exec-out cat /sdcard/window.xml > /tmp/leksika-android-ui.xml 2>/dev/null || true
    if grep -q 'Выполнено' /tmp/leksika-android-ui.xml && grep -q '17' /tmp/leksika-android-ui.xml; then
      echo 'APK запустил программу и вывел 17.'
      exit 0
    fi
    sleep 2
  done
  exit 1
)

if attempt || attempt; then
  exit 0
fi

echo '::warning::запуск APK не дал результата за две попытки'
# Разные коды возврата различимы в аннотации GitHub, а полный лог лежит в
# хранилище Azure и из некоторых окружений недоступен. Код 4 — разметки нет,
# код 5 — разметка есть, а результата в ней нет.
if [ -s /tmp/leksika-android-ui.xml ]; then
  cat /tmp/leksika-android-ui.xml
  echo 'Результат программы в WebView не найден' >&2
  exit 5
fi
echo 'Разметка WebView не получена' >&2
exit 4
