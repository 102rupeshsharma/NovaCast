# NovaCast shared emulator engine.
#
# Plain C++20 with no SDL, Windows or Android dependencies. Built by the
# desktop project (via the root CMakeLists.txt) and by the Android app
# (mobile/app/src/main/cpp/CMakeLists.txt). Each hardware module is its own
# static library depending only on novacast_common.

cmake_minimum_required(VERSION 3.21)

if(NOT COMMAND novacast_configure_target)
  include(${CMAKE_CURRENT_LIST_DIR}/../cmake/CompilerWarnings.cmake)
endif()

if(NOT DEFINED NOVACAST_VERSION_STRING)
  file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/../VERSION NOVACAST_VERSION_STRING LIMIT_COUNT 1)
endif()

# ---------------------------------------------------------------------------
# common: types, logging, error handling, file helpers. No hardware here.
# ---------------------------------------------------------------------------
add_library(novacast_common STATIC
  common/error.cpp
  common/fileio.cpp
  common/log.cpp
  common/module_info.cpp
)
target_include_directories(novacast_common PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_compile_features(novacast_common PUBLIC cxx_std_20)
target_compile_definitions(novacast_common PUBLIC NOVACAST_VERSION="${NOVACAST_VERSION_STRING}")
novacast_configure_target(novacast_common)

# ---------------------------------------------------------------------------
# Hardware modules. Each one depends only on `common`, never on another module.
# ---------------------------------------------------------------------------
set(NOVACAST_HARDWARE_MODULES cpu gpu audio memory disc controller)
set(NOVACAST_cpu_SOURCES cpu/sh4.cpp cpu/sh4_interp.cpp)
set(NOVACAST_memory_SOURCES memory/bus.cpp)
foreach(module IN LISTS NOVACAST_HARDWARE_MODULES)
  add_library(novacast_${module} STATIC ${module}/${module}_module.cpp ${NOVACAST_${module}_SOURCES})
  target_link_libraries(novacast_${module} PUBLIC novacast_common)
  novacast_configure_target(novacast_${module})
endforeach()

# The SH-4 FPU honours FPSCR.RM, so the interpreter changes the host rounding
# mode while it runs. The compiler must not assume round-to-nearest.
if(MSVC)
  target_compile_options(novacast_cpu PRIVATE /fp:strict)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(novacast_cpu PRIVATE -frounding-math)
else()
  # Clang (Android NDK): -frounding-math is supported from Clang 12 onwards.
  target_compile_options(novacast_cpu PRIVATE -frounding-math)
endif()

# ---------------------------------------------------------------------------
# system: owns the modules, the scheduler, and wires them together.
# ---------------------------------------------------------------------------
add_library(novacast_system STATIC system/emulator.cpp system/scheduler.cpp)
foreach(module IN LISTS NOVACAST_HARDWARE_MODULES)
  target_link_libraries(novacast_system PUBLIC novacast_${module})
endforeach()
novacast_configure_target(novacast_system)
