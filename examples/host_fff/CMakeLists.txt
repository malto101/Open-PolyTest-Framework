cmake_minimum_required(VERSION 3.20)
project(polyontest_host_fff C)

set(POLYONTEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

set(POLYONTEST_PROFILE "small" CACHE STRING "PolyOnTest size profile")
set(POLYONTEST_MINIMAL_PRINT ON CACHE BOOL "Human-readable output")

include(${POLYONTEST_ROOT}/cmake/PolyOnTest.cmake)

add_executable(host_fff_tests main.c)
target_include_directories(host_fff_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${POLYONTEST_ROOT}/plugins/extension/fff_fakes
)
target_link_libraries(host_fff_tests PRIVATE polyontest_core)
target_compile_features(host_fff_tests PRIVATE c_std_11)
