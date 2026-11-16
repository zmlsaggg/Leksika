cmake_minimum_required(VERSION 3.20)
project(leksika VERSION 0.6.0 LANGUAGES C)
# Один и тот же сценарий используется исходным деревом и overlay-портом.
include("${CMAKE_CURRENT_SOURCE_DIR}/packaging/vcpkg/ports/leksika/library.cmake")
