cmake_minimum_required(VERSION 3.20)
project(elecard_eye_mac CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Werror)
if(APPLE)
  add_compile_options(-Wno-deprecated-declarations)
endif()

include(${CMAKE_SOURCE_DIR}/cmake/Sanitizers.cmake)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Concurrent Test)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
                  libavformat libavcodec libavutil libswscale)

include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(CTest)
include(GoogleTest)

add_subdirectory(tests)
add_subdirectory(libs/io)
add_subdirectory(libs/parser)
add_subdirectory(libs/model)
add_subdirectory(libs/decoder)
add_subdirectory(app)
add_subdirectory(tools/dump_frames)
