cmake_minimum_required(VERSION 3.20)
project(polyontest_qemu_m33_smoke C ASM)

set(POLYONTEST_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(qemu_m33_tests.elf
  startup.c
  board_uart.c
  syscalls.c
  main.c
  ${POLYONTEST_ROOT}/harness/c/polyontest_core.c
  ${POLYONTEST_ROOT}/harness/c/polyontest_assert.c
)

target_include_directories(qemu_m33_tests.elf PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${POLYONTEST_ROOT}/harness/include
)

# Size profile: tiny | small | full (default small for firmware)
set(POLYONTEST_PROFILE "small" CACHE STRING "PolyOnTest size profile: tiny|small|full")
set_property(CACHE POLYONTEST_PROFILE PROPERTY STRINGS tiny small full)

# Freestanding (no newlib); profile selects COBS/tags/fixtures/float.
set(_pt_defs POLYONTEST_FREESTANDING)
if(POLYONTEST_PROFILE STREQUAL "tiny")
  list(APPEND _pt_defs POLYONTEST_PROFILE_TINY)
elseif(POLYONTEST_PROFILE STREQUAL "small")
  list(APPEND _pt_defs POLYONTEST_PROFILE_SMALL)
elseif(POLYONTEST_PROFILE STREQUAL "full")
  list(APPEND _pt_defs POLYONTEST_PROFILE_FULL POLYONTEST_EXCLUDE_FLOAT)
else()
  message(FATAL_ERROR "POLYONTEST_PROFILE must be tiny, small, or full")
endif()
target_compile_definitions(qemu_m33_tests.elf PRIVATE ${_pt_defs})
target_compile_options(qemu_m33_tests.elf PRIVATE
  -Wall
  -Wextra
  -Os
)

target_link_options(qemu_m33_tests.elf PRIVATE
  "-T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld"
  "-Wl,-Map=${CMAKE_BINARY_DIR}/qemu_m33_tests.map"
  "-Wl,--start-group"
  "-lgcc"
  "-Wl,--end-group"
)

set_target_properties(qemu_m33_tests.elf PROPERTIES
  OUTPUT_NAME "qemu_m33_tests"
  SUFFIX ".elf"
)

add_custom_command(TARGET qemu_m33_tests.elf POST_BUILD
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:qemu_m33_tests.elf>
  COMMENT "Firmware size"
)
