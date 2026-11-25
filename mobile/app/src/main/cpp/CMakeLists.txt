# Native part of the Android app: the shared NovaCast engine (../../../../../core)
# plus a thin JNI bridge. No emulation code lives here.

cmake_minimum_required(VERSION 3.22.1)
project(novacast_android LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(NOVACAST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
include(${NOVACAST_ROOT}/cmake/CompilerWarnings.cmake)
add_subdirectory(${NOVACAST_ROOT}/core ${CMAKE_CURRENT_BINARY_DIR}/novacast-core)

add_library(novacast_jni SHARED novacast_jni.cpp)
target_link_libraries(novacast_jni PRIVATE novacast_system log)
novacast_configure_target(novacast_jni)
