# Engineering Handoff: Local Pure-F407 Workspace

Updated: 2026-07-27

## Repository state

- Role: active local pure-F407 DJI C-board workspace
- Branch: `main`
- Materialized candidate commit:
  `924697c7312e0c8425f563bce13fe517cb0847b8`
- Superproject remote: none
- Cross-machine clone: deferred

## Source provenance

- Accepted firmware source:
  `bc31a22616c598d81b19d95c7c7d5877cb1a5f94`
- Accepted source-manifest commit:
  `48c6370b8d3e043f7f853a1c89ca2b4a3b79a525`
- Exporter commit:
  `8b1d1683b753d01e418d337df3155bf4a11801ce`
- Candidate canonical SHA-256:
  `7003205C6D77E65E796BFD4A7A1DFE10F5D5B0C3BAF2C6514AC15AB9446BE16D`

The local submodules were initialized through untracked overrides in this
repository's Git metadata. Tracked `.gitmodules` retains HTTPS URLs.

## Acceptance boundary

```text
PURE_F407_LOCAL_REPOSITORY=PASS
LOCAL_SUBMODULE_INITIALIZATION=PASS
PURE_F407_SOURCE_PROVENANCE=PASS
PURE_F407_LOCAL_SOFTWARE_ACCEPTED=PASS
MULTIBOARD_F407_HW1=PASS_HISTORICAL
PURE_F407_HW1=NOT_RUN
F407_CORE_RUNTIME=NOT_RUN
F407_USB_HARDWARE=HARDWARE_UNVERIFIED
F4_2_FRESH_CLONE=BLOCKED_BY_SUBMODULE_REACHABILITY_HISTORICAL
CROSS_MACHINE_CLONE=DEFERRED
F407_ONLY_TEAM_RELEASE=DEFERRED
```

The historical HW-1 evidence used a multiboard-produced Board Smoke ELF. This
repository's ELF has not been programmed. Retained historical details are
CMSIS-DAPv2 serial `482752132243`, SWD DPIDR `0x2ba01477`, Cortex-M4 r0p1,
STM32 device ID `0x413`, 1024 KiB Flash, and successful halt, program, verify,
and reset. `main`, ThreadX runtime, LED, DWT, UART, and reset-loop checks were
not run.

## Actual changes after deterministic materialization

- established the concise pure-F407 documentation chain;
- made build output directories stable for Board Smoke and USB CDC;
- added repository-relative VS Code Board Smoke and USB debug configurations;
- added a promoted-workspace static-check mode and four regression tests;
- registered the promoted Gate and regression tests in host CTest;
- retained ignored local build/validation logs under `build/logs/`;
- retained fail-closed descriptor and zero-motor defaults.

No firmware implementation, IOC, generated closure, public API, shared
submodule, or gitlink changed.

## Validation

Fresh local results and artifact hashes are recorded in
[docs/VALIDATION.md](docs/VALIDATION.md). Hardware validation remains
`NOT_RUN`.

All six F407 firmware presets built fresh with zero warnings; host CTest passed
34/34. The CubeMX production, board-boundary, no-H723, ELF/MAP,
descriptor-fail-closed, controller-stopped, motor-zero, licence, JSON,
Markdown, and tracked-cleanliness gates passed. The Board Smoke ELF is:

```text
build/dji-c-board/pnx_embedded.elf
SHA-256=1C8C9A0CF5C74DCD37A02346E43A942933701EA495AE7CB6C4106DA07A7CE192
```

`release/f407-only-provenance.json` is the immutable F4-2 export snapshot. Its
candidate-time `NOT_RUN` values are retained as provenance and are not the
ledger for this promoted workspace; current results are recorded here and in
`docs/VALIDATION.md`.

## Next action

`STEER-PURE-F407-HW1`: program, verify, and reset
`build/dji-c-board/pnx_embedded.elf`, retaining its SHA-256 and programmer
log.

## Rollback

The deterministic materialization/base commit is
`924697c7312e0c8425f563bce13fe517cb0847b8`; the current HEAD is the local
commit containing this handoff. No remote, push, tag, hardware, or Vault
action occurred.
