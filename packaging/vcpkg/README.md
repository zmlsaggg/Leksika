# Локальный порт vcpkg

Это **overlay-порт**, он ещё не опубликован в реестре Microsoft. Он собирает
статическую C11-библиотеку компилятора, значений, байткода и VM из закреплённого
коммита `36fe170c309baf2547c5c394c9642028f1815ebd` с проверкой SHA-512.
Сценарий сборки хранится в самом порте, поэтому работает с этим уже
опубликованным коммитом. Не используйте этот порт для проверки изменений ядра
в рабочем дереве: обновите закреплённую ревизию и хеш после публикации.

Библиотека — **подмножество** Лексики, не полный Python-исполнитель, не CLI и
не словарь. C/C++ API и ABI не объявлены стабильными между версиями; собирайте
потребителя и библиотеку совместимыми компилятором, архитектурой и CRT. VM
не является полноценной песочницей. Среду и разрешения надо передавать явно.
Динамическая библиотека не поддерживается этим портом.

## Установка overlay-порта

Установите vcpkg по официальной инструкции Microsoft. Из корня репозитория:

```sh
vcpkg install leksika --overlay-ports="$PWD/packaging/vcpkg/ports" --triplet=x64-linux
```

Windows PowerShell (статический CRT и библиотека):

```powershell
vcpkg install leksika --overlay-ports="$PWD/packaging/vcpkg/ports" --triplet=x64-windows-static
```

Triplet и инструменты должны соответствовать приложению. Для других архитектур
используйте подходящий статический triplet и проверьте его самостоятельно.
Библиотека не требует Python или C++ для сборки (`BUILD_TESTING=OFF`); C++17
нужен только потребителям обёртки и её тестам.

## CMake-проект потребителя

Укажите при конфигурации путь к `scripts/buildsystems/vcpkg.cmake` в установленном
vcpkg и тот же triplet. В `CMakeLists.txt`:

```cmake
find_package(leksika CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE leksika::leksika)
```

C: `#include <leksika/ядро/компилятор.h>` и
`#include <leksika/ядро/машина.h>`. C++17:
`#include <leksika/cpp/лексус.hpp>`. Рабочие примеры C и C++ и их сборка
находятся в `проверка/vcpkg/`. См. также `cpp/README.md` о владении ресурсами,
разрешениях, ограничениях и безопасности.

## Проверка исходного дерева без vcpkg

```sh
cmake -S . -B /tmp/leksika-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/leksika-build
ctest --test-dir /tmp/leksika-build --output-on-failure
cmake --install /tmp/leksika-build --prefix /tmp/leksika-install
cmake -S проверка/vcpkg -B /tmp/leksika-consumer -DCMAKE_PREFIX_PATH=/tmp/leksika-install
cmake --build /tmp/leksika-consumer
ctest --test-dir /tmp/leksika-consumer --output-on-failure
```

Для Visual Studio добавьте `--config Release` к командам build, install и
ctest (`ctest ... -C Release`). Это новая отдельная сборка библиотеки; старые
Makefile и сборка Windows EXE не изменены.
