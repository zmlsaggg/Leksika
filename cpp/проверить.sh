#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
cc=${CC:-cc}; cxx=${CXX:-c++}
# Имена выходных файлов ASCII: MinGW assembler не принимает Unicode в пути вывода.
index=0
for source in значения байткод машина компилятор; do
  "$cc" -std=c11 -O1 -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8 -c "$root/ядро/$source.c" -o "$tmp/core$index.o"
  index=$((index + 1))
done
"$cxx" -std=c++17 -O1 -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8 \
  "$root/cpp/проверка.cpp" "$tmp/core0.o" "$tmp/core1.o" "$tmp/core2.o" "$tmp/core3.o" -o "$tmp/cpp-test"
"$tmp/cpp-test"
echo 'C++ обёртка и C ABI проверены.'
