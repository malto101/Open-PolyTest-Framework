# PolyOnTest

**PolyOnTest** is an open-source, Apache-2.0 embedded-first test framework with a
SOLID plugin architecture. It targets hobby MCUs, RTOS apps, kernels, and host
libraries (C / C++ / Rust) without locking you into Zephyr or Pigweed.

## Thirty-second taste

```bash
python3 scripts/amalgamate.py
# copy dist/polyontest.h dist/polyontest.c into your project
```

```c
#define POLYONTEST_PROFILE_TINY
#define POLYONTEST_MINIMAL_PRINT
#include "polyontest.h"

TEST(Math, Basic, Add) { ASSERT_EQ(4, 2 + 2); }

int main(void) { return polyontest_run_all(); }
```

Compile `polyontest.c` with your existing Makefile/CMake — no PolyOnTest build system required.

## Documentation

Full guides, architecture diagrams, CLI reference, and language adapters live in
**[docs/](docs/)**. Build the site locally:

```bash
python3 -m venv .venv-docs
source .venv-docs/bin/activate
pip install -r docs/requirements.txt
mkdocs serve
```

| Topic | Link |
|-------|------|
| Quickstart | [docs/quickstart.md](docs/quickstart.md) |
| Concepts & progressive enhancement | [docs/concepts.md](docs/concepts.md) |
| Architecture | [docs/architecture.md](docs/architecture.md) |
| Size profiles | [docs/profiles.md](docs/profiles.md) |
| CLI | [docs/cli.md](docs/cli.md) |

## Examples

| Example | Path |
|---------|------|
| Host C | [examples/host_c](examples/host_c) |
| Host C++ | [examples/host_cpp](examples/host_cpp) |
| Host Rust | [examples/host_rust](examples/host_rust) |
| FFF mocks | [examples/host_fff](examples/host_fff) |
| QEMU Cortex-M33 | [examples/qemu_m33_smoke](examples/qemu_m33_smoke) |

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md), and
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

## License

Apache-2.0 — see [LICENSE](LICENSE) and [NOTICE](NOTICE).
