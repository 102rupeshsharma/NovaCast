# NovaCast desktop application (Windows). The only code that uses SDL2.

# Command-line parsing has no SDL dependency, so the tests link it directly.
add_library(novacast_cli STATIC src/command_line.cpp)
target_link_libraries(novacast_cli PUBLIC novacast_common)
target_include_directories(novacast_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
novacast_configure_target(novacast_cli)

add_executable(novacast
  src/main.cpp
  src/sdl_window.cpp
)
target_link_libraries(novacast PRIVATE novacast_system novacast_cli novacast_sdl2)
novacast_configure_executable(novacast)
