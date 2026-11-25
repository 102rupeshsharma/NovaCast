# ---------------------------------------------------------------------------
# common: types, logging, error handling, file helpers. No hardware here.
# ---------------------------------------------------------------------------
add_library(novacast_common STATIC
  common/error.cpp
  common/fileio.cpp
  common/log.cpp
  common/module_info.cpp
)
target_include_directories(novacast_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(novacast_common PUBLIC cxx_std_20)
target_compile_definitions(novacast_common PUBLIC NOVACAST_VERSION="${PROJECT_VERSION}")
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
else()
  target_compile_options(novacast_cpu PRIVATE -frounding-math)
endif()

# ---------------------------------------------------------------------------
# core: owns the modules and wires them together into a system.
# ---------------------------------------------------------------------------
add_library(novacast_core STATIC core/emulator.cpp core/scheduler.cpp)
foreach(module IN LISTS NOVACAST_HARDWARE_MODULES)
  target_link_libraries(novacast_core PUBLIC novacast_${module})
endforeach()
novacast_configure_target(novacast_core)

# ---------------------------------------------------------------------------
# app: the executable. The only target that knows about SDL2.
# ---------------------------------------------------------------------------
# Command-line parsing has no SDL dependency, so the tests link it directly.
add_library(novacast_cli STATIC app/command_line.cpp)
target_link_libraries(novacast_cli PUBLIC novacast_common)
target_include_directories(novacast_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/app)
novacast_configure_target(novacast_cli)

add_executable(novacast
  app/main.cpp
  app/sdl_window.cpp
)
target_link_libraries(novacast PRIVATE novacast_core novacast_cli novacast_sdl2)
novacast_configure_executable(novacast)
