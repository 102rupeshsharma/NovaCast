# Applies the project's warning and correctness flags to a target.
function(novacast_configure_target target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    if(NOVACAST_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
    if(NOVACAST_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()

  # NOVACAST_DEBUG is defined only in Debug builds; Release builds get NDEBUG from CMake.
  target_compile_definitions(${target} PRIVATE $<$<CONFIG:Debug>:NOVACAST_DEBUG=1>)
endfunction()

# MinGW executables are linked statically so they run without the MinGW runtime DLLs.
function(novacast_configure_executable target)
  novacast_configure_target(${target})
  if(MINGW)
    target_link_options(${target} PRIVATE -static)
  endif()
endfunction()
