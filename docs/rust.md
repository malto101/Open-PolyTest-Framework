# Rust adapter (`polyontest-rs`)

The [`polyontest-rs`](https://github.com/malto101/Open-PolyTest-Framework/tree/main/crates/polyontest-rs)
crate is a thin FFI-facing adapter. Default is `#![no_std]`; enable the `std`
feature for host helpers.

A `#[polyontest::test]` proc-macro is **future work** and not required for v1 —
register cases from C (`TEST` macros) and drive the runner from Rust.

## Host example

[`examples/host_rust`](https://github.com/malto101/Open-PolyTest-Framework/tree/main/examples/host_rust)
compiles the C harness + a small `tests.c` via `build.rs`, then calls
`run_from_env()` from Rust.

!!! note "Linker anchors"
    On macOS/ELF linkers, constructor registration in a static archive can be
    dead-stripped; the example calls `polyontest_host_rust_link_anchor()` so
    `tests.c` stays in the final binary.

```bash
# Human-readable
cargo run -p polyontest-host-rust

# Tag filter
POLYONTEST_TAG=unit cargo run -p polyontest-host-rust

# COBS + CLI
cargo build -p polyontest-host-rust --no-default-features --features cobs
cargo run -p polyontest -- run --target host \
  --config examples/host_rust/polyontest.toml
```

## API (`std` feature)

```rust
use polyontest_rs::std_support;

std_support::run_all();
std_support::run_tag("smoke");
std_support::run_suite("RustHost");
std_support::run_group("RustHost", "Basic");
std_support::run_from_env();
```

FFI symbols live under `polyontest_rs::ffi` (also usable from `no_std`).

## Features

| Feature | Default | Meaning |
|---------|---------|---------|
| (none) | `no_std` | Version marker + FFI signatures |
| `std` | off | Host helpers (`run_*`, `run_from_env`) |

See [CLI](cli.md) for structured reporting and [Architecture](architecture.md)
for how the host drains the stream.
