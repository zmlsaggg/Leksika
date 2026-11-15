#!/usr/bin/env bash
# Запускается из корня checkout; имя цели и тестовый запуск задаёт workflow.
set -euo pipefail
platform=${1:?нужна платформа}
case "$platform" in
  windows-x64) exe=leksus-core.exe; archive=leksika-native-windows-x64.zip ;;
  linux-x64|macos-arm64|macos-x64|android-arm64) exe=leksus-core; archive="leksika-native-${platform}.tar.gz" ;;
  *) echo "Неизвестная платформа: $platform" >&2; exit 1 ;;
esac
mkdir -p package release
cp "ядро/$exe" "package/$exe"
cp LICENSE package/LICENSE.txt
cp источники/russian-words/LICENSE package/DICTIONARY-LICENSE.txt
gzip -dc ядро/русские_формы.lxd.gz > package/dictionary.lxd
cp примеры/нативная_vm.lek package/example.lek
cp packaging/README-native.txt package/README.txt
if [[ "$platform" == windows-x64 ]]; then
  (cd package && 7z a -tzip "../release/$archive" "$exe" LICENSE.txt DICTIONARY-LICENSE.txt dictionary.lxd example.lek README.txt >/dev/null)
else
  chmod 755 "package/$exe"
  tar -C package -czf "release/$archive" "$exe" LICENSE.txt DICTIONARY-LICENSE.txt dictionary.lxd example.lek README.txt
fi
