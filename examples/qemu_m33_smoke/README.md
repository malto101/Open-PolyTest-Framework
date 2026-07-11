# QEMU M33 example (v1 on-target)

Freestanding Cortex-M33 firmware for **QEMU `mps2-an505`**.

## What it does

1. Boots from `0x10000000`, uses **32 KiB** SSRAM at `0x30000000` (stack top `0x30008000`)
2. Runs auto-registered `TEST` cases via `.init_array` constructors
3. Streams PolyOnTest **COBS/PTWP** frames via **ARM semihosting** (QEMU stderr)
4. Exits QEMU with semihosting `SYS_EXIT`

> Note: AN505 UART0 (`0x40200000`) sits behind TrustZone PPC; this smoke uses
> semihosting so CI needs no peripheral bring-up. UART sink remains a board-pack
> option once PPC setup is added.

## Build

```bash
# small (default) — tags/fixtures/COBS
cmake -S examples/qemu_m33_smoke -B build/qemu_m33 \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/examples/qemu_m33_smoke/toolchain-arm-none-eabi.cmake \
  -DPOLYONTEST_PROFILE=small

# tiny — text only, smallest text size
cmake -S examples/qemu_m33_smoke -B build/qemu_tiny \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/examples/qemu_m33_smoke/toolchain-arm-none-eabi.cmake \
  -DPOLYONTEST_PROFILE=tiny

cmake --build build/qemu_m33   # or build/qemu_tiny
```

Requires `arm-none-eabi-gcc` and `qemu-system-arm`. Post-build prints `arm-none-eabi-size`.

## Run

```bash
cargo run -p polyontest -- run --target qemu_m33 \
  --config examples/qemu_m33_smoke/polyontest.toml
```

Firmware is **~1.7 KB** text with `POLYONTEST_FREESTANDING` (no newlib).
