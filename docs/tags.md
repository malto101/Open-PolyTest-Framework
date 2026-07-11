# Tags and filters

## Declaring tags

Tags attach at suite, group, or case level (small/full profiles):

```c
POLYONTEST_SUITE_TAGS(Math, "host", "smoke");
POLYONTEST_GROUP_TAGS(Math, Basic, "unit");
TEST_TAGS(Math, Basic, SkipMe, "skipdemo") { IGNORE(); }
```

A case matches a tag filter if **any** of its own tags, its group tags, or its
suite tags contain the requested string.

!!! warning "Profile gated"
    Tags require `POLYONTEST_CFG_HAS_TAGS` (small/full). The tiny profile has no
    tag filtering — use separate binaries or hard-coded runner entry points.

## Running a subset

### In-process

```c
polyontest_run_tag("smoke");
polyontest_run_suite("Math");
polyontest_run_group("Math", "Basic");
polyontest_run_from_env();  // preferred for host binaries
```

### Environment (host)

| Variable | Effect |
|----------|--------|
| `POLYONTEST_TAG` | `polyontest_run_tag` |
| `POLYONTEST_SUITE` + `POLYONTEST_GROUP` | `polyontest_run_group` |
| `POLYONTEST_SUITE` alone | `polyontest_run_suite` |

Priority: tag → suite+group → suite → all.

```bash
POLYONTEST_TAG=unit ./build/host_c/host_c_tests
```

### CLI

```bash
cargo run -p polyontest -- run --target host \
  --config examples/host_c/polyontest.toml --tag smoke
```

See [CLI](cli.md). Filters are **host-only** in v0.1.
