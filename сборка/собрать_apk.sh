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
"$tools/aapt" package -f -M android/app/src/main/AndroidManifest.xml -I "$platform" -A "$work/assets" -F "$work/app-unsigned.apk"
javac -encoding UTF-8 -source 8 -target 8 -bootclasspath "$platform" -d "$work/classes" android/app/src/main/java/org/leksika/sandbox/MainActivity.java
(cd "$work/classes" && jar cf "$work/classes.jar" .)
"$tools/d8" --min-api 24 --lib "$platform" --output "$work/dex" "$work/classes.jar"
(cd "$work/dex" && "$tools/aapt" add "$work/app-unsigned.apk" classes.dex)
"$tools/zipalign" -f 4 "$work/app-unsigned.apk" "$work/aligned.apk"
# Временная подпись: сборка устанавливается, но не обновляет приложение с другим ключом.
keytool -genkeypair -keystore "$work/key.jks" -storepass android -keypass android -alias leksika \
  -keyalg RSA -keysize 2048 -validity 3650 -dname 'CN=Leksika temporary build key' >/dev/null 2>&1
"$tools/apksigner" sign --ks "$work/key.jks" --ks-pass pass:android --key-pass pass:android \
  --out release/01-Leksika-Android.apk "$work/aligned.apk"
"$tools/apksigner" verify --verbose release/01-Leksika-Android.apk
"$tools/aapt" dump badging release/01-Leksika-Android.apk | grep -F "package: name='org.leksika.sandbox'"
