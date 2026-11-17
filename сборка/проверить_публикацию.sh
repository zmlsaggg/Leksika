#!/usr/bin/env bash
# Сверяет GitHub API assets c файлами, подготовленными для публикации.
# sha256 сверяется всегда, когда GitHub его уже посчитал (.digest). Пока
# дайджеста нет, сравниваем размер: загрузка состоялась, а сумму всё равно
# подтверждает zz-SHA256SUMS.txt из самого релиза.
set -euo pipefail
metadata=${1:?нужен JSON ответа GitHub}
dir=${2:?нужен каталог релиза}
files=(00-Leksika-Windows.exe 01-Leksika-Android.apk zz-SHA256SUMS.txt zz-checksums.txt leksika-native-windows-x64.zip
       leksika-native-linux-x64.tar.gz leksika-native-macos-arm64.tar.gz
       leksika-native-macos-x64.tar.gz leksika-native-android-arm64.tar.gz)
# Расширение редактора необязательно: проверяем, только если оно есть локально.
if [[ -f "$dir/leksika-language.vsix" ]]; then
  files+=(leksika-language.vsix)
fi
for file in "${files[@]}"; do
  if [[ ! -f "$dir/$file" ]]; then
    echo "Нет файла в комплекте: $dir/$file" >&2; exit 1
  fi
  hash=$(sha256sum "$dir/$file"); hash=${hash%% *}
  size=$(wc -c < "$dir/$file"); size=${size//[[:space:]]/}
  asset_line=$(jq -r --arg name "$file" \
    '[.assets[] | select(.name == $name)][0]
     | [(.state // ""), (.digest // ""), ((.size // 0) | tostring)] | join("|")' \
    "$metadata")
  if [ -z "$asset_line" ]; then
    echo "На GitHub нет файла: $file" >&2; exit 1
  fi
  IFS='|' read -r state digest assetsize <<< "$asset_line"
  if [ "$state" != "uploaded" ]; then
    echo "Файл $file ещё не загружен (состояние: ${state:-нет})" >&2; exit 1
  fi
  if [ -n "$digest" ] && [ "$digest" != "sha256:$hash" ]; then
    echo "Не совпадает SHA-256: $file" >&2; exit 1
  fi
  if [ -z "$digest" ] && [ "$assetsize" != "$size" ]; then
    echo "Не совпадает размер: $file ($assetsize вместо $size)" >&2; exit 1
  fi
done
echo "Все файлы комплекта загружены и сверены."
