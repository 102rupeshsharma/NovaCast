# Third-party dependencies. Both are pinned to exact releases and fetched at
# configure time, so a fresh clone needs only CMake, a C++20 compiler and
# network access on the first configure.

include(FetchContent)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

# ---------------------------------------------------------------------------
# SDL2: windowing, input and (later) audio output.
# ---------------------------------------------------------------------------
if(NOVACAST_USE_SYSTEM_SDL2)
  find_package(SDL2 2.26 REQUIRED CONFIG)
else()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
  set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
  set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)

  FetchContent_Declare(SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz)
  FetchContent_MakeAvailable(SDL2)
endif()

# One name for the rest of the build, whichever way SDL2 was found.
add_library(novacast_sdl2 INTERFACE)
if(TARGET SDL2::SDL2-static)
  target_link_libraries(novacast_sdl2 INTERFACE SDL2::SDL2-static)
elseif(TARGET SDL2-static)
  target_link_libraries(novacast_sdl2 INTERFACE SDL2-static)
elseif(TARGET SDL2::SDL2)
  target_link_libraries(novacast_sdl2 INTERFACE SDL2::SDL2)
else()
  message(FATAL_ERROR "SDL2 was configured but no usable SDL2 target was found")
endif()
# NovaCast provides its own main(); SDL must not redefine it.
target_compile_definitions(novacast_sdl2 INTERFACE SDL_MAIN_HANDLED)

# ---------------------------------------------------------------------------
# GoogleTest: unit and integration tests.
# ---------------------------------------------------------------------------
if(NOVACAST_BUILD_TESTS)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip)
  FetchContent_MakeAvailable(googletest)
endif()
