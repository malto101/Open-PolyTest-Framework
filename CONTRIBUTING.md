# Contributing to PolyOnTest

Thanks for your interest in contributing.

## License

By contributing, you agree that your contributions will be licensed under the
Apache License 2.0.

## Development setup

```bash
# Rust toolchain (stable)
cargo build --workspace
cargo test --workspace

# Host C example
cmake -S examples/host_c -B build/host_c
cmake --build build/host_c
./build/host_c/host_c_tests

# Amalgamate drop-in
python3 scripts/amalgamate.py
```

## Design rules

1. **SOLID plugins** — new transports/codecs/boards/reporters are plugins; do not bake them into Core.
2. **Core stays tiny** — `harness/c` and the amalgam must remain `no_std`-friendly.
3. **Progressive enhancement** — hobby Core stream must not pay for Command/HIL features.
4. Prefer open-source dependencies only.

## Pull requests

- Keep PRs focused and documented.
- Add or update tests for behavior changes.
- Run host tests before opening a PR.
- Do not commit secrets or personal machine paths.
