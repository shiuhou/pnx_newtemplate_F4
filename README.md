# PnX DJI C-board F407

This is the active local pure-F407 workspace for ordinary DJI C-board
development and hardware bring-up. It was materialized from the accepted
deterministic candidate and retains four exact shared-module gitlinks.

Shared framework changes originate in `../pnx_h7_f4`; this repository owns the
F407-only product workflow and must not become a private fork of shared
modules.

```text
PURE_F407_LOCAL_REPOSITORY=PASS
PURE_F407_HW1=NOT_RUN
F407_CORE_RUNTIME=NOT_RUN
F407_USB_HARDWARE=HARDWARE_UNVERIFIED
USB_DESCRIPTOR_IDENTITY=UNASSIGNED_FAIL_CLOSED
F407_ONLY_TEAM_RELEASE=DEFERRED
```

## Build

```powershell
cmake --fresh --preset board-smoke
cmake --build --preset board-smoke
```

Board Smoke output:
`build/dji-c-board/pnx_embedded.elf`.

USB CDC software image:

```powershell
cmake --fresh --preset usb-cdc
cmake --build --preset usb-cdc
```

USB CDC output:
`build/dji-c-board-usb-cdc/pnx_embedded.elf`.

All six firmware presets are listed in
[docs/VALIDATION.md](docs/VALIDATION.md). Host tests use:

```powershell
cmake --fresh --preset host-tests
cmake --build --preset host-tests
ctest --preset host-tests
```

The 2026-07-27 local software run built all six F407 presets and passed 34/34
host tests. The next image for manual verification is
`build/dji-c-board/pnx_embedded.elf`, SHA-256
`1C8C9A0CF5C74DCD37A02346E43A942933701EA495AE7CB6C4106DA07A7CE192`.

## Safety

- Motor output is zero by default.
- USB descriptor identity is unassigned and controller start remains
  fail-closed.
- A successful build is not physical runtime evidence.
- Do not push, add a publication remote, or use hardware without explicit
  authorization.

Start with [HANDOFF.md](HANDOFF.md) and
[docs/CURRENT_TASK.md](docs/CURRENT_TASK.md). Source identity is recorded in
[the export provenance](release/f407-only-provenance.json).
