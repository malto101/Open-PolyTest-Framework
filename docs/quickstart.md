# Quickstart

Three common paths. Pick a tab and follow the commands.

=== "Hobby / MCU"

    No CLI required — amalgamate, drop in two files, compile with your toolchain.

    1. Generate the amalgam:

        ```bash
        python3 scripts/amalgamate.py
        ```

    2. Copy `dist/polyontest.h` and `dist/polyontest.c` into your project.

    3. Define a profile (and optional text-only path):

        ```c
        #define POLYONTEST_PROFILE_TINY
        #define POLYONTEST_MINIMAL_PRINT
        #include "polyontest.h"

        TEST(Math, Basic, Add) { ASSERT_EQ(4, 2 + 2); }

        int main(void) { return polyontest_run_all(); }
        ```

    4. Compile `polyontest.c` with your Makefile/CMake and read PASS/FAIL on
       serial or stdout.

    See [Profiles](profiles.md) for size trade-offs and freestanding writer hooks.

    ### Parameterized cases (small/full)

    ```c
    typedef struct { int a, b, sum; } row_t;
    static const row_t k_rows[] = {
        {1, 1, 2},
        {2, 3, 5},
    };

    PARAM_TEST(Math, Basic, AddTable, row_t, k_rows) {
        const row_t row = PARAM_AS(row_t);
        ASSERT_EQ(row.sum, row.a + row.b);
    }
    ```

    Failures append `[param=<index>]`. Inside a normal `TEST`,
    `FOR_EACH(type, var, array)` also sets the param cursor.

=== "Host + CLI"

    Structured COBS stream into console, JUnit, and JSON reporters.

    ```bash
    cmake -S examples/host_c -B build/host_c \
      -DPOLYONTEST_MINIMAL_PRINT=OFF -DPOLYONTEST_PROFILE=full
    cmake --build build/host_c
    cargo run -p polyontest -- run --target host \
      --config examples/host_c/polyontest.toml
    ```

    Produces `report.xml` and `report.json` plus a console summary.

    Filter by tag (host only):

    ```bash
    cargo run -p polyontest -- run --target host \
      --config examples/host_c/polyontest.toml --tag smoke
    ```

    Tiny host smoke (no CLI):

    ```bash
    cmake -S examples/host_c -B build/host_tiny \
      -DPOLYONTEST_PROFILE=tiny -DPOLYONTEST_MINIMAL_PRINT=ON
    cmake --build build/host_tiny && ./build/host_tiny/host_c_tests
    ```

    Full filter and toml reference: [CLI](cli.md).

=== "QEMU Cortex-M33"

    On-target smoke under `mps2-an505` with semihosting stream.

    ```bash
    # Needs arm-none-eabi-gcc + qemu-system-arm
    cargo run -p polyontest -- run --target qemu_m33 \
      --config examples/qemu_m33_smoke/polyontest.toml
    ```

    Build with `-DPOLYONTEST_PROFILE=tiny` or `small` to compare firmware size.

    !!! warning "Filters on QEMU"
        Freestanding QEMU builds have no `getenv`. CLI `--tag` / `--suite` /
        `--group` are rejected for `qemu_m33`. Hard-code `polyontest_run_tag` or
        `polyontest_run_suite` in the example `main` if you need a subset.

    Typical loop: edit → fast host check → QEMU in CI → (later) desk hardware.

## C++ / Rust

- C++: [C++ adapter](cpp.md) — `examples/host_cpp`
- Rust: [Rust adapter](rust.md) — `examples/host_rust`

## Mocking (FFF)

```bash
cmake -S examples/host_fff -B build/host_fff
cmake --build build/host_fff && ./build/host_fff/host_fff_tests
```

See [Mocking](mocking.md).

## Next

- [Concepts](concepts.md) — which path and how lifecycle/CI fit together
- [Architecture](architecture.md) — diagrams of host vs DUT
