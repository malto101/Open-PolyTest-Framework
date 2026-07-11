# Size profiles

PolyOnTest Core can strip features at compile time so the same harness fits a
hobby MCU (tiny) or a host CI binary (full).

## Choosing a profile

```mermaid
flowchart TD
  start([Pick a profile]) --> flash{Flash / RAM tight?}
  flash -->|Very tight ~1–3 KB| tiny[POLYONTEST_PROFILE_TINY]
  flash -->|Moderate MCU| small[POLYONTEST_PROFILE_SMALL]
  flash -->|Host CI or roomy target| full[POLYONTEST_PROFILE_FULL]
  tiny --> textOnly[Text path only — no tags / fixtures / float]
  small --> knobsSmall{Need float asserts?}
  knobsSmall -->|No| smallDefault[Float off by default]
  knobsSmall -->|Yes| enableFloat[Do not set EXCLUDE_FLOAT carefully]
  full --> knobsFull{Multithreaded host?}
  knobsFull -->|Yes| mutex[polyontest_set_locks]
  knobsFull -->|No| fullDefault[Default full features]
  textOnly --> ortho[Orthogonal knobs below]
  smallDefault --> ortho
  enableFloat --> ortho
  mutex --> ortho
  fullDefault --> ortho
```

| Profile | Define | Typical size | Features |
|---------|--------|--------------|----------|
| **tiny** | `POLYONTEST_PROFILE_TINY` | ~1–3 KB text | Text output only; no tags; no suite/group fixtures; no float; no longjmp |
| **small** | `POLYONTEST_PROFILE_SMALL` | mid | Hierarchy + tags + fixtures + COBS (unless `POLYONTEST_MINIMAL_PRINT`); float off by default; protect/abort OK |
| **full** | `POLYONTEST_PROFILE_FULL` or unset | largest | Floats (unless `POLYONTEST_EXCLUDE_FLOAT`), tags, hierarchy, COBS, protect, optional mutex hooks |

Derived macros (from `polyontest_profile.h`):

- `POLYONTEST_CFG_HAS_COBS`
- `POLYONTEST_CFG_HAS_TAGS`
- `POLYONTEST_CFG_HAS_FIXTURES`
- `POLYONTEST_CFG_HAS_FLOAT`
- `POLYONTEST_CFG_HAS_PROTECT`
- `POLYONTEST_CFG_HAS_MUTEX`
- `POLYONTEST_CFG_HAS_EXTENDED_ASSERTS` (string/memory/bits/arrays; off in tiny)
- `POLYONTEST_CFG_HAS_HEAP` (when `POLYONTEST_USE_HEAP`)

## CMake

```bash
cmake -S examples/host_c -B build/host_tiny \
  -DPOLYONTEST_PROFILE=tiny -DPOLYONTEST_MINIMAL_PRINT=ON
cmake -S examples/qemu_m33_smoke -B build/qemu_tiny \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/examples/qemu_m33_smoke/toolchain-arm-none-eabi.cmake \
  -DPOLYONTEST_PROFILE=tiny
```

Or `include(cmake/PolyOnTest.cmake)` after setting `POLYONTEST_PROFILE`.

## Orthogonal knobs

| Knob | Effect |
|------|--------|
| `POLYONTEST_MINIMAL_PRINT` | Force text path (no COBS) |
| `POLYONTEST_EXCLUDE_FLOAT` | Drop float/double asserts |
| `POLYONTEST_NO_LONGJMP` | PROTECT always succeeds; ABORT only sets fail |
| `POLYONTEST_USE_HEAP` | Enable `polyontest_register_heap_case` |
| `POLYONTEST_USE_SECTION_REGISTRY` | Place cases in `.polyontest_info` |
| `POLYONTEST_FREESTANDING` | No stdio; set writer yourself |

!!! tip "Freestanding"
    On bare metal, call `polyontest_set_writer` before `polyontest_run_*` so events
    reach your UART / semihosting sink.

## Mutex hooks (full)

```c
void polyontest_set_locks(polyontest_lock_fn_t lock, polyontest_lock_fn_t unlock, void *user);
```

When set, Core wraps assert fail-flag updates and writer emit. No-ops if NULL.
Intended for multithreaded **host** runners under the full profile.

## Heap registration

```c
#define POLYONTEST_USE_HEAP
int polyontest_register_heap_case(const char *suite, const char *group,
                                const char *name, polyontest_fn_t fn);
```

Static ctor lists remain the default.

## Section registry (GNU ld)

Default discovery uses `__attribute__((constructor))`. For section-based
registration:

1. Compile with `-DPOLYONTEST_USE_SECTION_REGISTRY`
2. Keep the section in the linker script:

```ld
.polyontest_info : {
  PROVIDE(__start_polyontest_info = .);
  KEEP(*(.polyontest_info))
  PROVIDE(__stop_polyontest_info = .);
} > FLASH
```

3. At run start, Core walks `__start_polyontest_info` … `__stop_polyontest_info`
   (GNU/Clang non-Apple). Host builds can keep the ctor path and omit the script.

## Size table (reference)

See [`examples/profile_sizes/README.md`](https://github.com/malto101/Open-PolyTest-Framework/blob/main/examples/profile_sizes/README.md)
for measured QEMU/host sizes after a local build.
