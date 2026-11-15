#!/usr/bin/env bash
# Из одного проверенного набора создаёт контрольные суммы для всех файлов.
set -euo pipefail
cd "${1:?нужен каталог артефактов}"
files=(leksika.exe 01-Leksika-Android.apk leksika-native-windows-x64.zip leksika-native-linux-x64.tar.gz
       leksika-native-macos-arm64.tar.gz leksika-native-macos-x64.tar.gz
       leksika-native-android-arm64.tar.gz)
for file in "${files[@]}"; do
  if [[ ! -f "$file" || ! -s "$file" ]]; then
    echo "Отсутствует или пуст файл: $file" >&2; exit 1
  fi
done
# Установщик уже проверен в отдельном Windows smoke-тесте и имеет свою сумму.
# Не принимаем неожиданные записи из переданного файла сумм.
if [[ $(wc -l < SHA256SUMS.txt) -ne 1 ]] || ! grep -Eq '^[0-9a-f]{64}  leksika\.exe$' SHA256SUMS.txt; then
  echo 'Неверный файл контрольной суммы установщика' >&2; exit 1
fi
sha256sum -c SHA256SUMS.txt
mv leksika.exe 00-Leksika-Windows.exe
files[0]=00-Leksika-Windows.exe
sha256sum "${files[@]:2}" > zz-checksums.txt
sha256sum "${files[@]}" > zz-SHA256SUMS.txt
rm SHA256SUMS.txt
sha256sum -c zz-SHA256SUMS.txt
sha256sum -c zz-checksums.txt
