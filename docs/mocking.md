# Mocking with PolyOnTest

PolyOnTest does not ship a Ruby/codegen mock generator. For C unit tests that need
fakes, use the header-only helpers in
[`plugins/extension/fff_fakes/polyontest_fff.h`](https://github.com/malto101/Open-PolyTest-Framework/blob/main/plugins/extension/fff_fakes/polyontest_fff.h).

## vs CMock

| | PolyOnTest FFF helpers | CMock |
|--|----------------------|-------|
| Codegen | None (macros in the test TU) | Ruby + YAML/C headers |
| Dependencies | Header only | Unity + CMock scripts |
| Style | [FFF](https://github.com/meekrosoft/fff)-like | Expect/Return API |
| Best for | Small HAL / driver seams | Large generated HAL mocks |

Use fakes when the seam is a C function you can redefine in the test binary
(same TU or weak symbol / link-order override). Prefer real hardware or a board
plugin when the behavior under test *is* the peripheral.

## Quick pattern

```c
#define POLYONTEST_MINIMAL_PRINT
#include "polyontest/polyontest.h"
#include "polyontest_fff.h"
#include "sensor.h"

POLYONTEST_FAKE_VALUE_FUNC1(int32_t, sensor_read, int, -1)

TEST(Hal, Sensor, ReadOnce) {
    POLYONTEST_FAKE_RESET_VALUE1(sensor_read, -1);
    sensor_read_return = 1200;
    ASSERT_EQ(1200, sensor_read(2));
    ASSERT_EQ(1, sensor_read_call_count);
    ASSERT_EQ(2, sensor_read_arg0_val);
}
```

Macros:

- `POLYONTEST_FAKE_VALUE_FUNC0/1/2`
- `POLYONTEST_FAKE_VOID_FUNC0/1/2`
- `POLYONTEST_FAKE_RESET` / `POLYONTEST_FAKE_RESET_VALUE1/2` / `POLYONTEST_FAKE_RESET_VOID1/2`
- Optional `fn_name##_custom_fake` function pointer for a custom body

Define `POLYONTEST_FFF_ALIASES` for short `FAKE_*` / `RESET_FAKE` names.

## Example

```bash
cmake -S examples/host_fff -B build/host_fff
cmake --build build/host_fff
./build/host_fff/host_fff_tests
```

!!! tip "Suggested profile"
    Use **small** (or default full). Tiny works if you avoid float asserts and
    do not rely on suite fixtures.
