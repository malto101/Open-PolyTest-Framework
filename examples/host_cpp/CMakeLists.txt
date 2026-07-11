cmake_minimum_required(VERSION 3.20)
project(polyontest_host_cpp LANGUAGES C CXX)

set(POLYONTEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(host_cpp_tests
  main.cpp
  ${POLYONTEST_ROOT}/harness/c/polyontest_core.c
  ${POLYONTEST_ROOT}/harness/c/polyontest_assert.c
)

target_include_directories(host_cpp_tests PRIVATE
  ${POLYONTEST_ROOT}/harness/include
  ${POLYONTEST_ROOT}/harness/cpp
)

set(POLYONTEST_PROFILE "full" CACHE STRING "PolyOnTest size profile: tiny|small|full")
set_property(CACHE POLYONTEST_PROFILE PROPERTY STRINGS tiny small full)

if(POLYONTEST_PROFILE STREQUAL "tiny")
  target_compile_definitions(host_cpp_tests PRIVATE POLYONTEST_PROFILE_TINY)
elseif(POLYONTEST_PROFILE STREQUAL "small")
  target_compile_definitions(host_cpp_tests PRIVATE POLYONTEST_PROFILE_SMALL)
else()
  target_compile_definitions(host_cpp_tests PRIVATE POLYONTEST_PROFILE_FULL)
endif()

option(POLYONTEST_MINIMAL_PRINT "Human-readable output" ON)
if(POLYONTEST_MINIMAL_PRINT OR POLYONTEST_PROFILE STREQUAL "tiny")
  target_compile_definitions(host_cpp_tests PRIVATE POLYONTEST_MINIMAL_PRINT)
endif()

target_compile_features(host_cpp_tests PRIVATE cxx_std_17 c_std_11)
set_source_files_properties(
  ${POLYONTEST_ROOT}/harness/c/polyontest_core.c
  ${POLYONTEST_ROOT}/harness/c/polyontest_assert.c
  PROPERTIES LANGUAGE C
)
