#!/usr/bin/env bash
set -Eeuo pipefail
trap 'echo "::error::Android smoke: $BASH_COMMAND (строка $LINENO)"' ERR
mkdir -p android-check
tar -xzf release/leksika-native-android-arm64.tar.gz -C android-check
adb shell 'mkdir -p /data/local/tmp/leksika'
adb push android-check/leksus-core android-check/example.lek android-check/dictionary.lxd /data/local/tmp/leksika/
adb shell 'chmod 755 /data/local/tmp/leksika/leksus-core'
adb shell '/data/local/tmp/leksika/leksus-core --self-test'
adb shell '/data/local/tmp/leksika/leksus-core выполнить /data/local/tmp/leksika/example.lek' | grep -q 'Готово!'
adb shell '/data/local/tmp/leksika/leksus-core слово книга /data/local/tmp/leksika/dictionary.lxd' | grep -q 'Слово есть'
