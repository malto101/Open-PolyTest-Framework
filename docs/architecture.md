# Architecture

PolyOnTest splits an **on-target C harness** from a **Rust host CLI**. They share
one wire model: PTWP events framed as COBS binary or plain text. Outer plugins
depend inward; **core never imports** a concrete UART, USB, board, or reporter.

## System context

```mermaid
flowchart TB
  subgraph target [DUT / on-target]
    harness["C harness\n(harness/c + amalgam)"]
    tests[User TEST cases]
    writer["Writer hook\n(stdout or polyontest_set_writer)"]
    tests --> harness
    harness --> writer
  end
  subgraph wire [Byte stream]
    ptwp["PTWP events\n(COBS or text lines)"]
  end
  subgraph host [Host composition root]
    cli["polyontest CLI"]
    board[Board plugin]
    transport[Transport plugin]
    codec[Codec plugin]
    reporters[Reporters]
    cli --> board
    board --> transport
    transport --> codec
    codec --> reporters
  end
  writer --> ptwp
  ptwp --> transport
  reporters --> artifacts["report.xml / report.json / console"]
```

## Container dependencies

```mermaid
flowchart TB
  cli["polyontest-cli\n(composition root)"]
  api["polyontest-plugin-api\n(traits)"]
  proto["polyontest-protocol\n(Event / MsgType)"]
  builtins["polyontest-builtins\n(stdio, cobs, text,\nhost, qemu_m33,\nconsole, junit, json)"]
  domain["harness/c\n(on-target domain)"]
  adapters["C++ / polyontest-rs\n(thin ABI wrappers)"]
  cli --> api
  api --> proto
  builtins -.->|implements| api
  cli --> builtins
  adapters --> domain
  domain -.->|emits events| proto
```

| Layer | Location | Role |
|-------|----------|------|
| Composition root | `crates/polyontest-cli` | Load toml, select plugins, drain until `Done` |
| Plugin traits | `crates/polyontest-plugin-api` | `Transport`, `Codec`, `Board`, `Reporter`, `ExtensionPack` |
| Builtins | `crates/polyontest-builtins` | In-tree host/QEMU/codec/reporter impls |
| Protocol | `crates/polyontest-protocol` | Codec-agnostic `Event` enum |
| On-target domain | `harness/c`, `harness/include` | Runner, asserts, registration |
| Drop-in amalgam | `dist/polyontest.h`, `dist/polyontest.c` | Generated via `scripts/amalgamate.py` |

## SOLID mapping

| Principle | Application |
|-----------|-------------|
| S | One plugin kind per concern |
| O | Add HCI/nanopb/Pico as plugins without editing Core |
| L | Any `Transport` / `Codec` is interchangeable |
| I | Separate traits — not one mega-plugin |
| D | CLI depends on traits; `polyontest.toml` selects impls |

## On-target vs host

| Side | Mechanism |
|------|-----------|
| Host | Rust traits + in-tree builtins |
| Target | Compile-time hooks (`polyontest_set_writer`, section/ctors) — no dlopen |

!!! note "QEMU `transport = \"uart\"`"
    For `qemu_m33`, the logical transport id is `uart`, but I/O is **semihosting
    written to QEMU stderr**, not a real UART peripheral. See the QEMU example
    board glue under `examples/qemu_m33_smoke/`.

## Host run sequence

```mermaid
sequenceDiagram
  participant User
  participant CLI as polyontest CLI
  participant Board
  participant Transport
  participant Codec
  participant Reporter
  participant DUT as DUT process
  User->>CLI: polyontest run --target …
  CLI->>Board: prepare / resolve artifact
  opt build command in toml
    Board->>DUT: shell build
  end
  CLI->>Transport: open (spawn child / QEMU)
  Transport->>DUT: start process
  loop until Event::Done
    DUT-->>Transport: bytes (stdout or stderr)
    Transport->>Codec: decode_feed
    Codec->>Reporter: on_event
  end
  Reporter->>Reporter: finish
  CLI-->>User: exit 0 or 1
```

## On-target emit path

```mermaid
flowchart LR
  subgraph runner [run_filtered]
    collect[Collect / sort cases]
    emitSuite[SUITE_START / END]
    emitCase[CASE_START + PASS/FAIL/SKIP]
    emitDone[DONE summary]
  end
  subgraph sink [Output]
    defaultOut[Default stdout writer]
    custom["polyontest_set_writer(...)"]
  end
  subgraph framing [Framing]
    cobs["COBS + PTWP binary\n(default when not MINIMAL_PRINT)"]
    text["Text lines\n(SUITE_START, PASS, DONE …)"]
  end
  collect --> emitSuite --> emitCase --> emitDone
  emitSuite --> defaultOut
  emitCase --> defaultOut
  emitDone --> defaultOut
  emitSuite --> custom
  emitCase --> custom
  emitDone --> custom
  defaultOut --> cobs
  defaultOut --> text
  custom --> cobs
  custom --> text
```

## PTWP events

Structured results use COBS-framed PTWP payloads (`codec = "cobs"`). Hobbyists
can use `POLYONTEST_MINIMAL_PRINT` / `codec = "text"` instead.

| Event (protocol) | Typical meaning |
|------------------|-----------------|
| `SuiteStart` / `SuiteEnd` | Suite boundary |
| `CaseStart` / `CaseEnd` | Case boundary with status |
| `AssertFail` | Failed assertion detail |
| `Log` | Diagnostic line |
| `Done` | Terminal counts — CLI stops draining |

## Size profiles and discovery

Compile-time profiles (`POLYONTEST_PROFILE_TINY` / `SMALL` / `FULL`) map to
`POLYONTEST_CFG_HAS_*` feature macros. See [Profiles](profiles.md).

Default discovery uses `__attribute__((constructor))`. Optional
`POLYONTEST_USE_SECTION_REGISTRY` walks `.polyontest_info` / `__DATA,polyontest`.
Linker details live on the profiles page.

## Related

- [Concepts](concepts.md) — progressive enhancement and lifecycle
- [Roadmap](roadmap.md) — isolation, HIL, coverage, chaos (design)
- [Plugins](plugins.md) — authoring builtins
- [CLI](cli.md) — toml schema and filters
