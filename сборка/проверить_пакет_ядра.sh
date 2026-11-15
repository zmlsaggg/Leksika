#!/usr/bin/env bash
set -Eeuo pipefail
trap 'echo "::error::Ошибка на строке $LINENO: $BASH_COMMAND"' ERR
platform=${1:?нужна платформа}
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
case "$platform" in
  windows-x64) 7z x -y "release/leksika-native-$platform.zip" "-o$tmp" >/dev/null; exe=leksus-core.exe ;;
  *) tar -xzf "release/leksika-native-$platform.tar.gz" -C "$tmp"; exe=leksus-core ;;
esac
cd "$tmp"
test -s dictionary.lxd && test -s LICENSE.txt && test -s DICTIONARY-LICENSE.txt && test -s README.txt
if [[ "$platform" == android-arm64 ]]; then
  readelf -h "$exe" | grep -q 'AArch64'
  readelf -h "$exe" | grep -q 'DYN'
  if readelf -l "$exe" | grep -q INTERP; then echo 'Android: неожиданный динамический загрузчик' >&2; exit 1; fi
  run=(qemu-aarch64 "./$exe")
else
  run=("./$exe")
fi
"${run[@]}" --self-test
"${run[@]}" выполнить example.lek | grep -q 'Готово!'
"${run[@]}" слово книга dictionary.lxd | grep -q 'Слово есть'
