# C++ adapter

PolyOnTest’s C++ surface is a thin wrapper over the C ABI. Keep writing
`TEST` / `ASSERT_*` with the C macros; call runners through `namespace polyontest`.

## Include

```cpp
#include "polyontest.hpp"   // finds harness/cpp + polyontest/polyontest.h
```

## Runners

```cpp
polyontest::run_all();
polyontest::run_tag("smoke");
polyontest::run_suite("Math");
polyontest::run_group("Math", "Basic");
polyontest::run_from_env();  // POLYONTEST_TAG / SUITE / GROUP
```

On the full profile, `polyontest::set_locks(...)` forwards to `polyontest_set_locks`.

## Example

```bash
cmake -S examples/host_cpp -B build/host_cpp -DPOLYONTEST_PROFILE=full
cmake --build build/host_cpp
./build/host_cpp/host_cpp_tests

# Tag filter via env (or CLI — see CLI)
POLYONTEST_TAG=unit ./build/host_cpp/host_cpp_tests
```

`PARAM_TEST` works the same as in C when using the small/full profile.

See [Quickstart](quickstart.md) and [Tags](tags.md).
