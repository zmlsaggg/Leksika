#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
cc=${CC:-cc}; cxx=${CXX:-c++}
for source in значения байткод машина компилятор; do
  "$cc" -std=c11 -O1 -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8 -c "$root/ядро/$source.c" -o "$tmp/$source.o"
done
"$cxx" -std=c++17 -O1 -Wall -Wextra -Wpedantic -finput-charset=UTF-8 -fexec-charset=UTF-8 \
  "$root/cpp/проверка.cpp" "$tmp/значения.o" "$tmp/байткод.o" "$tmp/машина.o" "$tmp/компилятор.o" -o "$tmp/cpp-test"
"$tmp/cpp-test"
echo 'C++ обёртка и C ABI проверены.'
