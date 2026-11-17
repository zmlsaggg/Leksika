#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
sdk=${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}
: "${sdk:?Нужен Android SDK}"
tools="$sdk/build-tools/35.0.0"
platform="$sdk/platforms/android-35/android.jar"
for tool in aapt d8 zipalign apksigner; do test -x "$tools/$tool"; done
test -f "$platform"
work=$(mktemp -d); trap 'rm -rf "$work"' EXIT
mkdir -p "$work/classes" "$work/dex" "$work/assets" release
cp android/app/src/main/assets/* "$work/assets/"
cp учебная-машина.js "$work/assets/machine.js"
# Словарь форм нужен полному исполнителю: при первом запуске приложение
# разворачивает сжатый файл в свои данные и передаёт ядру путь.
cp ядро/русские_формы.lxd.gz "$work/assets/dictionary.lxd.gz"
ls -l "$work/assets/dictionary.lxd.gz"
# Нативное ядро кладётся как библиотека: Android распаковывает такие файлы в
# каталог приложения, откуда их можно запускать как обычную программу.
# Формат переменной — пары «архитектура=путь» через пробел. Две архитектуры
# нужны потому, что проверка идёт в эмуляторе x86_64, а телефоны — arm64.
cores=${LEKSUS_CORES:-}
if [ -n "$cores" ]; then
  for pair in $cores; do
    abi=${pair%%=*}; binary=${pair#*=}
    case "$abi" in
      arm64-v8a|x86_64|x86) ;;
      *) echo "Неизвестная архитектура APK: $abi" >&2; exit 1 ;;
    esac
    test -f "$binary"
    mkdir -p "$work/lib/$abi"
    cp "$binary" "$work/lib/$abi/libleksus-core.so"
    echo "APK с нативным ядром: $abi"
  done
else
  echo '::warning::LEKSUS_CORES не задан: APK соберётся без нативного ядра, останется песочница'
fi
"$tools/aapt" package -f -M android/app/src/main/AndroidManifest.xml -I "$platform" -S android/app/src/main/res -A "$work/assets" -F "$work/app-unsigned.apk"
javac -encoding UTF-8 -source 8 -target 8 -bootclasspath "$platform" -d "$work/classes" \
  android/app/src/main/java/org/leksika/sandbox/MainActivity.java \
  android/app/src/main/java/org/leksika/sandbox/NativeRunnerActivity.java
(cd "$work/classes" && jar cf "$work/classes.jar" .)
"$tools/d8" --min-api 24 --lib "$platform" --output "$work/dex" "$work/classes.jar"
(cd "$work/dex" && "$tools/aapt" add "$work/app-unsigned.apk" classes.dex)
if [ -n "$cores" ]; then
  # Без сжатия: запускаемый файл нельзя прятать в deflate. -p выравнивает
  # библиотеки по странице, как требует упаковка нативного кода.
  for pair in $cores; do
    abi=${pair%%=*}
    (cd "$work" && zip -0 -q "$work/app-unsigned.apk" "lib/$abi/libleksus-core.so")
  done
fi
"$tools/zipalign" -f -p 4 "$work/app-unsigned.apk" "$work/aligned.apk"
# Временная подпись: сборка устанавливается, но не обновляет приложение с другим ключом.
keytool -genkeypair -keystore "$work/key.jks" -storepass android -keypass android -alias leksika \
  -keyalg RSA -keysize 2048 -validity 3650 -dname 'CN=Leksika temporary build key' >/dev/null 2>&1
"$tools/apksigner" sign --ks "$work/key.jks" --ks-pass pass:android --key-pass pass:android \
  --out release/01-Leksika-Android.apk "$work/aligned.apk"
"$tools/apksigner" verify --verbose release/01-Leksika-Android.apk
"$tools/aapt" dump badging release/01-Leksika-Android.apk > "$work/badging.txt"
grep -F "package: name='org.leksika.sandbox'" "$work/badging.txt"
grep -F 'application-icon-' "$work/badging.txt"
# Содержимое APK проверяем разбором архива: сообщения получаются понятными, а
# список файлов виден в журнале, если чего-то не хватает.
LEKSUS_CORES="$cores" python3 сборка/проверить_содержимое_apk.py release/01-Leksika-Android.apk
echo "размер APK: $(wc -c < release/01-Leksika-Android.apk) байт"
