#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"
files=(leksika.exe 01-Leksika-Android.apk leksika-native-windows-x64.zip leksika-native-linux-x64.tar.gz
       leksika-native-macos-arm64.tar.gz leksika-native-macos-x64.tar.gz
       leksika-native-android-arm64.tar.gz)
for file in "${files[@]}"; do printf 'fixture %s\n' "$file" > "$file"; done
sha256sum leksika.exe > SHA256SUMS.txt
rm "${files[2]}"
if bash "$root/сборка/подготовить_релиз.sh" "$tmp" >/dev/null 2>&1; then echo 'Пропущен отсутствующий файл' >&2; exit 1; fi
printf 'fixture %s\n' "${files[2]}" > "${files[2]}"
printf 'bad\n' > SHA256SUMS.txt
if bash "$root/сборка/подготовить_релиз.sh" "$tmp" >/dev/null 2>&1; then echo 'Пропущена неверная сумма' >&2; exit 1; fi
sha256sum leksika.exe > SHA256SUMS.txt
bash "$root/сборка/подготовить_релиз.sh" "$tmp" >/dev/null
files[0]=00-Leksika-Windows.exe
test "$(wc -l < zz-SHA256SUMS.txt)" -eq 7
test "$(wc -l < zz-checksums.txt)" -eq 5
sha256sum -c zz-SHA256SUMS.txt >/dev/null
# Модель ответа GitHub проверяет отсутствие и повреждение опубликованных файлов.
jq -n '{assets:[]}' > metadata.json
for file in "${files[@]}" zz-SHA256SUMS.txt zz-checksums.txt; do
  hash=$(sha256sum "$file"); hash=${hash%% *}
  jq --arg name "$file" --arg digest "sha256:$hash" '.assets += [{name:$name,state:"uploaded",digest:$digest}]' metadata.json > next.json
  mv next.json metadata.json
done
bash "$root/сборка/проверить_публикацию.sh" metadata.json "$tmp"
jq 'del(.assets[0])' metadata.json > missing.json
if bash "$root/сборка/проверить_публикацию.sh" missing.json "$tmp" >/dev/null 2>&1; then echo 'Пропущен отсутствующий ресурс GitHub' >&2; exit 1; fi
printf corrupt >> "${files[3]}"
if bash "$root/сборка/проверить_публикацию.sh" metadata.json "$tmp" >/dev/null 2>&1; then echo 'Пропущена неверная сумма GitHub' >&2; exit 1; fi
if sha256sum -c zz-SHA256SUMS.txt >/dev/null 2>&1; then echo 'Изменённый архив не обнаружен' >&2; exit 1; fi
echo 'Комплект релиза и контрольные суммы проверены.'
