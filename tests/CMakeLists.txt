include(GoogleTest)

add_executable(novacast_tests
  test_helpers.h
  common/test_error.cpp
  common/test_log.cpp
  common/test_fileio.cpp
  system/test_modules.cpp
  system/test_emulator.cpp
  system/test_scheduler.cpp
  memory/test_bus.cpp
  cpu/sh4_harness.h
  cpu/test_sh4_integer.cpp
  cpu/test_sh4_control.cpp
  cpu/test_sh4_fpu.cpp
  desktop/test_command_line.cpp
)
target_link_libraries(novacast_tests PRIVATE novacast_system novacast_cli GTest::gtest_main)
target_include_directories(novacast_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
novacast_configure_executable(novacast_tests)

# PRE_TEST discovery lists the tests when ctest runs, not at build time.
gtest_discover_tests(novacast_tests DISCOVERY_MODE PRE_TEST)

# Application smoke tests: the real executable must start and exit cleanly.
add_test(NAME app.version COMMAND novacast --version)
set_tests_properties(app.version PROPERTIES PASS_REGULAR_EXPRESSION "NovaCast ${PROJECT_VERSION}")

add_test(NAME app.status COMMAND novacast --status)
set_tests_properties(app.status PROPERTIES PASS_REGULAR_EXPRESSION "Can run Dreamcast software: no")

add_test(NAME app.bad_option COMMAND novacast --no-such-option)
set_tests_properties(app.bad_option PROPERTIES WILL_FAIL TRUE)

add_test(NAME app.missing_game COMMAND novacast --smoke-test does-not-exist.gdi)
set_tests_properties(app.missing_game PROPERTIES WILL_FAIL TRUE)

# Opens a hidden SDL window, presents three frames and shuts down.
add_test(NAME app.window_smoke COMMAND novacast --smoke-test)
set_tests_properties(app.window_smoke PROPERTIES
  PASS_REGULAR_EXPRESSION "shut down cleanly after 3 frames"
  TIMEOUT 30)
