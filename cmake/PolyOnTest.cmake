# PolyOnTest CMake helpers (FetchContent / add_subdirectory)
# SPDX-License-Identifier: Apache-2.0
#
# Usage:
#   set(POLYONTEST_PROFILE tiny)   # or small / full
#   option(POLYONTEST_MINIMAL_PRINT ... ON)
#   include(cmake/PolyOnTest.cmake)
#   target_link_libraries(my_tests PRIVATE polyontest_core)

if(NOT TARGET polyontest_core)
  add_library(polyontest_core STATIC
    "${CMAKE_CURRENT_LIST_DIR}/../harness/c/polyontest_core.c"
    "${CMAKE_CURRENT_LIST_DIR}/../harness/c/polyontest_assert.c"
  )
  target_include_directories(polyontest_core PUBLIC
    "${CMAKE_CURRENT_LIST_DIR}/../harness/include"
  )
  target_compile_features(polyontest_core PUBLIC c_std_11)

  if(NOT DEFINED POLYONTEST_PROFILE)
    set(POLYONTEST_PROFILE "full")
  endif()
  if(POLYONTEST_PROFILE STREQUAL "tiny")
    target_compile_definitions(polyontest_core PUBLIC POLYONTEST_PROFILE_TINY)
  elseif(POLYONTEST_PROFILE STREQUAL "small")
    target_compile_definitions(polyontest_core PUBLIC POLYONTEST_PROFILE_SMALL)
  elseif(POLYONTEST_PROFILE STREQUAL "full")
    target_compile_definitions(polyontest_core PUBLIC POLYONTEST_PROFILE_FULL)
  endif()

  if(POLYONTEST_MINIMAL_PRINT OR POLYONTEST_PROFILE STREQUAL "tiny")
    target_compile_definitions(polyontest_core PUBLIC POLYONTEST_MINIMAL_PRINT)
  endif()
endif()
