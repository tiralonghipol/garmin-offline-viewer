cmake_minimum_required(VERSION 3.23)  # 3.23 for FILE_SET HEADERS

project(fitviewer
    VERSION 0.1.0
    DESCRIPTION "Garmin FIT file decoder and Qt viewer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # for clangd / clang-tidy

option(FITVIEWER_BUILD_GUI   "Build the Qt viewer application" ON)
option(FITVIEWER_BUILD_TOOLS "Build the fitdump command-line tool" ON)
option(FITVIEWER_BUILD_TESTS "Build the GoogleTest suite" ON)
option(FITVIEWER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(CompilerWarnings)
include(Version)

# Core library first: everything else depends on it.
add_subdirectory(libs/fit)

if(FITVIEWER_BUILD_TOOLS)
    add_subdirectory(tools/fitdump)
endif()

if(FITVIEWER_BUILD_GUI)
    add_subdirectory(app)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        include(AppImage)
    endif()
endif()

if(FITVIEWER_BUILD_TESTS)
    enable_testing()  # must be in the top-level file for ctest to find tests
    add_subdirectory(tests)
endif()
