cmake_minimum_required(VERSION 3.21)

file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION NOVACAST_VERSION_STRING LIMIT_COUNT 1)

project(NovaCast
  VERSION ${NOVACAST_VERSION_STRING}
  DESCRIPTION "An independent Sega Dreamcast emulator"
  LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not supported. Use a separate build directory or a preset.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Single-config generators (Ninja, Makefiles) default to Debug.
get_property(NOVACAST_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT NOVACAST_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug, Release, RelWithDebInfo)" FORCE)
endif()

option(NOVACAST_BUILD_TESTS "Build the NovaCast test suite (downloads GoogleTest)" ON)
option(NOVACAST_USE_SYSTEM_SDL2 "Use an installed SDL2 (find_package) instead of downloading it" OFF)
option(NOVACAST_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

# Put every executable in one folder so tests can find the app.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Shared emulator engine (also built by the Android app).
add_subdirectory(core)
# Windows desktop application.
add_subdirectory(desktop)

if(NOVACAST_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
