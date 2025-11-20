cmake_minimum_required(VERSION 3.22)

if(APPLE AND NOT DEFINED CMAKE_C_COMPILER)
  if(EXISTS /opt/homebrew/opt/llvm/bin/clang)
    set(CMAKE_C_COMPILER /opt/homebrew/opt/llvm/bin/clang)
    set(CMAKE_CXX_COMPILER /opt/homebrew/opt/llvm/bin/clang++)
    message(STATUS "Using Homebrew LLVM")
  endif()
endif()
project(LooperRoot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Only set Debug if not already specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

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
  8.0.10
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

# Compiler flags based on platform and build type
if(MSVC)
  add_compile_options(/Wall /WX)
  # SIMD for MSVC
  if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    add_compile_options(/arch:AVX2)
    add_compile_definitions(__AVX2__)
    message(STATUS "SIMD: Enabled AVX2 (MSVC)")
  endif()
elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
  # M1/M2/M3 Macs - don't use -march=native, let it use NEON naturally
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    add_compile_options(-O3)
    message(STATUS "SIMD: ARM64 (M-series Mac) - NEON enabled by default")
  else()
    add_compile_options(-O1)
    message(STATUS "SIMD: ARM64 Debug build with -O1")
  endif()
else()
  # Linux/Intel
  add_compile_options(-Wall -Wextra -Wpedantic)
  if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    add_compile_options(-march=native -O3)
    add_compile_definitions(__AVX2__)
    message(STATUS "SIMD: Enabled native arch with -march=native (GCC/Clang)")
  else()
    add_compile_options(-march=native -O1)
    message(STATUS "SIMD: Debug build with -march=native -O1")
  endif()
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  option(ENABLE_COVERAGE "Enable test coverage reporting" ON)

  if(ENABLE_COVERAGE)
    message(STATUS "Building with coverage instrumentation")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
      add_compile_options(-O0 -g -fprofile-instr-generate -fcoverage-mapping
                          -fno-inline -fno-omit-frame-pointer)
      add_link_options(-fprofile-instr-generate)
    else()
      message(FATAL_ERROR "Coverage only supported on Clang/GCC")
    endif()
  endif()

endif()

add_subdirectory(plugin)
# add_subdirectory(test)
