include(GNUInstallDirs)
include(CTest)

# CLI (лексус.c) намеренно не входит в библиотеку и не изменяется этой сборкой.
# Сценарий общий для исходного дерева и для закреплённой опубликованной
# ревизии. В той ревизии морфологии ещё нет, поэтому её участие проверяется
# наличием файла: иначе порт не собрался бы из архива старого тега.
set(leksika_sources
  ядро/значения.c ядро/байткод.c ядро/машина.c ядро/компилятор.c)
set(leksika_headers
  ядро/значения.h ядро/байткод.h ядро/машина.h ядро/компилятор.h)
set(leksika_tests значения машина компилятор контракт)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ядро/морфология.c")
  # Компилятор использует падежную морфологию, поэтому она часть библиотеки.
  list(APPEND leksika_sources ядро/морфология.c)
  list(APPEND leksika_headers ядро/морфология.h)
  list(APPEND leksika_tests морфология)
endif()

add_library(leksika STATIC ${leksika_sources})
add_library(leksika::leksika ALIAS leksika)
set_target_properties(leksika PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
target_include_directories(leksika INTERFACE "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
if(MSVC)
  target_compile_options(leksika PRIVATE /utf-8 /Zc:preprocessor)
endif()

install(TARGETS leksika EXPORT leksika-targets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES ${leksika_headers}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/leksika/ядро")
install(FILES cpp/лексус.hpp DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/leksika/cpp")
install(EXPORT leksika-targets NAMESPACE leksika:: DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/leksika")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/leksika-config.cmake"
  "include(\"\${CMAKE_CURRENT_LIST_DIR}/leksika-targets.cmake\")\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/leksika-config.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/leksika")

if(BUILD_TESTING)
  set(test_number 0)
  foreach(test IN LISTS leksika_tests)
    math(EXPR test_number "${test_number} + 1")
    add_executable(test_${test_number} "ядро/${test}_проверка.c")
    set_target_properties(test_${test_number} PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES C_EXTENSIONS NO)
    target_link_libraries(test_${test_number} PRIVATE leksika)
    if(MSVC)
      target_compile_options(test_${test_number} PRIVATE /utf-8 /Zc:preprocessor)
    endif()
    add_test(NAME test_${test_number} COMMAND test_${test_number})
  endforeach()
  enable_language(CXX)
  add_executable(test_cpp cpp/проверка.cpp)
  target_compile_features(test_cpp PRIVATE cxx_std_17)
  target_link_libraries(test_cpp PRIVATE leksika)
  if(MSVC)
    target_compile_options(test_cpp PRIVATE /utf-8 /Zc:preprocessor /UNDEBUG)
  else()
    target_compile_options(test_cpp PRIVATE -UNDEBUG)
  endif()
  add_test(NAME test_cpp COMMAND test_cpp)
endif()
