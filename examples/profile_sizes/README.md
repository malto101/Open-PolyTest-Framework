# Profile size notes

Measured on macOS with `arm-none-eabi-gcc` 16.1 / QEMU `mps2-an505` smoke
(`examples/qemu_m33_smoke`, freestanding, `-Os`).

```bash
cmake -S examples/qemu_m33_smoke -B build/qemu_$PROFILE \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/examples/qemu_m33_smoke/toolchain-arm-none-eabi.cmake \
  -DPOLYONTEST_PROFILE=$PROFILE
cmake --build build/qemu_$PROFILE
arm-none-eabi-size build/qemu_$PROFILE/qemu_m33_tests.elf
```

| Target | Profile | text | data | bss | Notes |
|--------|---------|------|------|-----|-------|
| qemu_m33 | tiny | **2791** | 84 | 44 | Text emit; no tags/fixtures/float/extended asserts |
| qemu_m33 | small | **2669** | 84 | 52 | COBS + hierarchy (default for CLI) |
| qemu_m33 | full | **2737** | 84 | 64 | Same smoke + mutex hooks compiled in (`EXCLUDE_FLOAT` still set for freestanding) |

Harness object sizes (tiny): `polyontest_assert.o` ≈ 1.5 KB, `polyontest_core.o` ≈ 1.6 KB.
On this tiny smoke firmware, **COBS (small) can beat text (tiny)** for total ELF size;
tiny still strips features and extended asserts for apps that would otherwise pull them in.

Host smoke (`examples/host_c`, Apple Clang) passes under `tiny` and `full`.
