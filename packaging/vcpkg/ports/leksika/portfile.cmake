vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

# Зафиксированная опубликованная ревизия, а не перемещающаяся ветка main.
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO zmlsaggg/Leksika
    REF 36fe170c309baf2547c5c394c9642028f1815ebd
    SHA512 1b904240178afb55565474f96f3127d0c8229f0a4bc7789a177ce3e1040fd7b76158a7f8614479e37f372a1405cead133db227f67f609cce0d19c764575adf77
)

# У опубликованной ревизии нет библиотечного CMake. Сценарий порта общий
# с CMakeLists.txt исходного дерева, чтобы не поддерживать две реализации.
file(COPY "${CMAKE_CURRENT_LIST_DIR}/library.cmake" DESTINATION "${SOURCE_PATH}")
file(WRITE "${SOURCE_PATH}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.20)\nproject(leksika VERSION 0.6.0 LANGUAGES C)\ninclude(\${CMAKE_CURRENT_SOURCE_DIR}/library.cmake)\n")
vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}" OPTIONS -DBUILD_TESTING=OFF)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/leksika)
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
