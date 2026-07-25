# PnX DJI C-board F407 Team Template

This repository is an unpublished F407-only candidate derived from the PnX
multi-board authority. Shared modules remain pinned Git submodules; fixes to
shared code must be made and reviewed in the authoritative integration
repository before a new export.

```text
F407_ONLY_HARDWARE=HARDWARE_UNVERIFIED
F407_ONLY_TEAM_RELEASE=NOT_PUBLISHED
USB_DESCRIPTOR_IDENTITY=UNASSIGNED_FAIL_CLOSED
```

No physical F407 runtime, USB enumeration, bidirectional CDC transfer or
reconnect result is implied by a successful build.

## Clone

```powershell
git clone --recurse-submodules <approved-repository-url>
cd <cloned-directory>
```

If the repository was cloned without submodules:

```powershell
git submodule update --init --recursive
```

## Build

Board smoke debug:

```powershell
cmake --fresh --preset board-smoke
cmake --build --preset board-smoke
```

USB CDC debug:

```powershell
cmake --fresh --preset usb-cdc
cmake --build --preset usb-cdc
```

All supported firmware presets:

```powershell
python scripts/check_cubemx_production.py --list-f407-presets
.\scripts\build_dji_c_board.ps1
```

Host tests:

```powershell
.\scripts\test_host.ps1
```

## Safety

- Default motor builds produce zero current.
- Never enable the non-zero motor compile gate without the separately approved
  bench procedure, explicit CAN bus, feedback ID, motor model and bounded
  current.
- The USB controller remains stopped while descriptor identity is unassigned.
- Do not substitute an STMicroelectronics VID or an arbitrary public VID/PID.
- Hardware work requires a separate approved Gate.

See [Team Guide](docs/TEAM_GUIDE.md) and
[candidate provenance](release/f407-only-provenance.json).
