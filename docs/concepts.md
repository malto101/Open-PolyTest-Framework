# Concepts

How PolyTest fits together: progressive enhancement, when to choose each path,
how the amalgam is built, and how CI exercises the matrix.

## Progressive enhancement

| Workflow | Name | Audience | Status |
|----------|------|----------|--------|
| Auto-register | Linker/ctor test discovery | Everyone | **v0.1** |
| Core stream | Boot → run → stream results | Hobby / MCU / host | **v0.1** |
| Size profiles | tiny / small / full | MCU flash budgets | **v0.1** |
| FFF mocks | Header-only fakes | Host unit tests | **v0.1** |
| Command | Boot → listen → host RPC | HIL / RTOS | v2 |
| HIL conductor | Main DUT + aux stimulator | Multi-board HIL | v2.x |

v0.1 is **stream mode only**: the DUT runs tests and emits events; the host
CLI drains that stream into reporters. Command mode and multi-board HIL are
planned extensions that reuse the same plugin traits.

## Which path should I use?

```mermaid
flowchart TD
  start([Need tests?]) --> hostOrMcu{Primary target?}
  hostOrMcu -->|Desktop / CI library| hostPath[Host binary + optional CLI]
  hostOrMcu -->|Bare metal / RTOS app| mcuPath{Need structured reports?}
  mcuPath -->|No — serial PASS/FAIL| hobby[Amalgam + TINY + MINIMAL_PRINT]
  mcuPath -->|Yes — JUnit/JSON in CI| qemuOrHw{Run under QEMU first?}
  qemuOrHw -->|Yes| qemuPath[qemu_m33 board + COBS stream]
  qemuOrHw -->|Desk hardware later| hwNote[Same writer hook; board plugin v2+]
  hostPath --> filters{Need tag filters?}
  filters -->|Yes| fromEnv[polytest_run_from_env + CLI flags]
  filters -->|No| runAll[polytest_run_all]
  hobby --> profiles[See Profiles]
  qemuPath --> profiles
  fromEnv --> plugins[See CLI and Plugins]
  runAll --> plugins
```

| Path | Docs |
|------|------|
| Amalgam drop-in | [Quickstart](quickstart.md) · [Profiles](profiles.md) |
| Host + CLI | [CLI](cli.md) · [Architecture](architecture.md) |
| QEMU on-target | [Quickstart](quickstart.md) (QEMU tab) · [example README](https://github.com/malto101/Open-PolyTest-Framework/blob/main/examples/qemu_m33_smoke/README.md) |
| Language adapters | [C++](cpp.md) · [Rust](rust.md) |

## Amalgamate vs modular CMake

Two ways to consume the C harness:

```mermaid
flowchart LR
  subgraph sources [Harness sources]
    hdr["harness/include/polytest/*.h"]
    src["harness/c/*.c"]
  end
  subgraph amalgamPath [Hobby / third-party MCU]
    script["scripts/amalgamate.py"]
    dist["dist/polytest.h + polytest.c"]
    userMake[Your Makefile or CMake]
  end
  subgraph modularPath [In-tree examples / CI]
    cmake["cmake/PolyTest.cmake"]
    lib[polytest_core static lib]
    examples[examples/*/]
  end
  hdr --> script
  src --> script
  script --> dist
  dist --> userMake
  hdr --> cmake
  src --> cmake
  cmake --> lib
  lib --> examples
```

- **Amalgam** — copy two files; no PolyTest build system on the DUT.
- **Modular** — link `polytest_core` via `PolyTest.cmake` when developing inside
  this repo or mirroring the example layout.

!!! tip "Generated files"
    `dist/*` is produced by amalgamate and marked do-not-edit. Change
    `harness/` sources, then re-run `python3 scripts/amalgamate.py`.

## Test lifecycle

Registration happens before `main`. The runner walks suites and cases, emits
events, then returns a process exit code.

```mermaid
flowchart TD
  ctor[Constructor / section registry] --> main[main]
  main --> entry{Runner entry}
  entry -->|run_all| all[match all]
  entry -->|run_tag / suite / group| filt[match filter]
  entry -->|run_from_env| env[POLYTEST_TAG / SUITE / GROUP]
  all --> loop
  filt --> loop
  env --> loop
  loop[Collect and sort cases] --> suiteOpen{New suite?}
  suiteOpen -->|Yes| suiteSetup[Suite teardown / setup + SUITE_START]
  suiteOpen -->|No| caseStart
  suiteSetup --> caseStart[CASE_START]
  caseStart --> groupSetup[Group setup]
  groupSetup --> body[Case body]
  body --> groupTeardown[Group teardown]
  groupTeardown --> status{Result}
  status -->|ok| pass[PASS]
  status -->|assert fail| fail[FAIL]
  status -->|IGNORE| skip[SKIP]
  pass --> more{More cases?}
  fail --> more
  skip --> more
  more -->|Yes| suiteOpen
  more -->|No| suiteEnd[SUITE_END]
  suiteEnd --> done["DONE passed=… failed=… skipped=…"]
  done --> exitCode[Return 0 or 1]
```

## CI matrix

Upstream CI (`.github/workflows/ci.yml`) mirrors the two main integration
paths:

```mermaid
flowchart TD
  push[Push / PR] --> hostJob[Job: host]
  push --> qemuJob[Job: qemu-m33]
  hostJob --> amalg[amalgamate.py]
  amalg --> cargo[cargo build / test]
  cargo --> hostC[host_c text / tiny / COBS+CLI]
  hostC --> fff[host_fff]
  fff --> cpp[host_cpp]
  cpp --> rust[host_rust + CLI]
  rust --> artsHost[Upload report.xml / report.json]
  qemuJob --> toolchains[arm-none-eabi-gcc + qemu-system-arm]
  toolchains --> qemuRun["polytest run --target qemu_m33"]
  qemuRun --> artsQemu[Upload reports]
```

Locally, the same commands appear in [Quickstart](quickstart.md).

## Next

- [Architecture](architecture.md) — host vs target, PTWP, plugin dependency rule
- [Roadmap](roadmap.md) — future tiers: isolation, HIL, coverage, chaos
- [Profiles](profiles.md) — flash budget knobs
- [Plugins](plugins.md) — extending the CLI composition root