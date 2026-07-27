# PnX DJI C-board F407

This is the active local pure-F407 workspace for ordinary DJI C-board
development and hardware bring-up. It was materialized from the accepted
deterministic candidate and retains four exact shared-module gitlinks.

Shared framework changes originate in `../pnx_h7_f4`; this repository owns the
F407-only product workflow and must not become a private fork of shared
modules.

```text
PURE_F407_LOCAL_REPOSITORY=PASS
PURE_F407_HW1=PASS
F407_CORE_RUNTIME=PASS
SIX_F407_PRESET_BUILDS=RETAINED_PASS
CURRENT_COMBINED_TREE_REGRESSION=NOT_RUN
HOST_TEST_BASELINE=RETAINED_34_OF_34_PASS
EXPANDED_CURRENT_HOST_SUITE=NOT_RUN
F407_USB_CDC=ISOLATED_LAB_PASS
USB_DESCRIPTOR_IDENTITY=UNASSIGNED_FAIL_CLOSED
DAILY_INTEGRATED_FIRMWARE=PARTIAL
F407_ONLY_TEAM_RELEASE=NOT_PUBLISHED
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

The retained 2026-07-27 baseline built all six F407 presets and passed 34/34
host tests. Those results do not represent a regression run of the exact
current combined tree. Individual USB, CAN1, M2006, PWM, BMI088 and IST8310
labs have since produced hardware evidence; formal common sensor runtime and
the current combined regression remain incomplete. The isolated USB identity
is lab-only and is not authorized for release; tracked presets remain
fail-closed.

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
