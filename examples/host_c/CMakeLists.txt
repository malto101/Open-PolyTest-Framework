cmake_minimum_required(VERSION 3.20)
project(polyontest_host_c C)

set(POLYONTEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(host_c_tests
  main.c
  ${POLYONTEST_ROOT}/harness/c/polyontest_core.c
  ${POLYONTEST_ROOT}/harness/c/polyontest_assert.c
)

target_include_directories(host_c_tests PRIVATE
  ${POLYONTEST_ROOT}/harness/include
)

# Size profile: tiny | small | full (default full when unset / "full")
set(POLYONTEST_PROFILE "full" CACHE STRING "PolyOnTest size profile: tiny|small|full")
set_property(CACHE POLYONTEST_PROFILE PROPERTY STRINGS tiny small full)

if(POLYONTEST_PROFILE STREQUAL "tiny")
  target_compile_definitions(host_c_tests PRIVATE POLYONTEST_PROFILE_TINY)
elseif(POLYONTEST_PROFILE STREQUAL "small")
  target_compile_definitions(host_c_tests PRIVATE POLYONTEST_PROFILE_SMALL)
elseif(POLYONTEST_PROFILE STREQUAL "full")
  target_compile_definitions(host_c_tests PRIVATE POLYONTEST_PROFILE_FULL)
else()
  message(FATAL_ERROR "POLYONTEST_PROFILE must be tiny, small, or full (got '${POLYONTEST_PROFILE}')")
endif()

# Default host demo uses human-readable output; switch off for COBS+CLI.
# Tiny profile forces text output regardless.
option(POLYONTEST_MINIMAL_PRINT "Human-readable output" ON)
if(POLYONTEST_MINIMAL_PRINT OR POLYONTEST_PROFILE STREQUAL "tiny")
  target_compile_definitions(host_c_tests PRIVATE POLYONTEST_MINIMAL_PRINT)
endif()

target_compile_features(host_c_tests PRIVATE c_std_11)

# Target for testing crash behavior
add_executable(host_c_crash_tests
  main.c
  ${POLYONTEST_ROOT}/harness/c/polyontest_core.c
  ${POLYONTEST_ROOT}/harness/c/polyontest_assert.c
)
target_compile_definitions(host_c_crash_tests PRIVATE POLYONTEST_TEST_CRASH POLYONTEST_PROFILE_FULL POLYONTEST_MINIMAL_PRINT)
target_include_directories(host_c_crash_tests PRIVATE ${POLYONTEST_ROOT}/harness/include)
target_compile_features(host_c_crash_tests PRIVATE c_std_11)

# Target for testing hang/timeout behavior
add_executable(host_c_hang_tests
  main.c
  ${POLYONTEST_ROOT}/harness/c/polyontest_core.c
  ${POLYONTEST_ROOT}/harness/c/polyontest_assert.c
)
target_compile_definitions(host_c_hang_tests PRIVATE POLYONTEST_TEST_HANG POLYONTEST_PROFILE_FULL POLYONTEST_MINIMAL_PRINT)
target_include_directories(host_c_hang_tests PRIVATE ${POLYONTEST_ROOT}/harness/include)
target_compile_features(host_c_hang_tests PRIVATE c_std_11)

