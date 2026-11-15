#!/usr/bin/env bash
# Сверяет GitHub API assets c файлами, подготовленными для публикации.
set -euo pipefail
metadata=${1:?нужен JSON ответа GitHub}
dir=${2:?нужен каталог релиза}
files=(leksika.exe SHA256SUMS.txt checksums.txt leksika-native-windows-x64.zip
       leksika-native-linux-x64.tar.gz leksika-native-macos-arm64.tar.gz
       leksika-native-macos-x64.tar.gz leksika-native-android-arm64.tar.gz)
for file in "${files[@]}"; do
  hash=$(sha256sum "$dir/$file"); hash=${hash%% *}
  if ! jq -e --arg name "$file" --arg hash "sha256:$hash" \
    '[.assets[] | select(.name == $name and .state == "uploaded" and .digest == $hash)] | length == 1' "$metadata" >/dev/null; then
    echo "На GitHub отсутствует файл или не совпадает SHA-256: $file" >&2; exit 1
  fi
done
