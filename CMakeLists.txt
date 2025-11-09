cmake_minimum_required(VERSION 3.22)

project(LooperRoot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs)
set(CPM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set_property(GLOBAL PROPERTY USE_FOLDERS YES)
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS ON)
set(JUCE_BUILD_EXTRAS OFF)
set(JUCE_BUILD_EXAMPLES OFF)

# Download CPM.cmake
file(
  DOWNLOAD
  https://github.com/cpm-cmake/CPM.cmake/releases/download/v0.42.0/CPM.cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake
  EXPECTED_HASH
    SHA256=2020b4fc42dba44817983e06342e682ecfc3d2f484a581f11cc5731fbe4dce8a)

include(${CPM_DIR}/CPM.cmake)

cpmaddpackage(
  NAME
  JUCE
  GITHUB_REPOSITORY
  juce-framework/JUCE
  GIT_TAG
  develop
  SOURCE_DIR
  ${LIB_DIR}/juce)

cpmaddpackage(
  NAME
  GOOGLETEST
  GITHUB_REPOSITORY
  google/googletest
  VERSION
  1.13.0
  SOURCE_DIR
  ${LIB_DIR}/googletest
  OPTIONS
  "INSTALL_GTEST OFF"
  "gtest_force_shared_crt ON")

set(BUILD_SOUNDSTRETCH
    OFF
    CACHE BOOL "" FORCE)
set(SOUNDTOUCH_DLL
    OFF
    CACHE BOOL "" FORCE)
cpmaddpackage(
  NAME
  Soundtouch
  URL
  https://www.surina.net/soundtouch/soundtouch-2.4.0.tar.gz
  SOURCE_DIR
  ${LIB_DIR}/soundtouch
  OPTIONS
  "BUILD_SOUNDSTRETCH OFF"
  "SOUNDTOUCH_DLL OFF")

# add package for json
cpmaddpackage("gh:nlohmann/json@3.12.0")

enable_testing()

if(MSVC)
  add_compile_options(/Wall /WX)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(ENABLE_COVERAGE "Enable test coverage reporting" OFF)

if(ENABLE_COVERAGE)
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(FATAL_ERROR "Coverage requires Debug build type")
  endif()

  message(STATUS "Building with coverage instrumentation")
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(-O0 -g -fprofile-instr-generate -fcoverage-mapping
                        -fno-inline -fno-omit-frame-pointer)
    add_link_options(-fprofile-instr-generate)
  else()
    message(FATAL_ERROR "Coverage only supported on Clang/GCC")
  endif()
endif()

add_subdirectory(plugin)
# add_subdirectory(test)
