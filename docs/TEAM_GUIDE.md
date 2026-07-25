# F407 Team Guide

## Current acceptance boundary

```text
F407_ONLY_HARDWARE=HARDWARE_UNVERIFIED
F407_ONLY_TEAM_RELEASE=NOT_PUBLISHED
USB_DESCRIPTOR_IDENTITY=UNASSIGNED_FAIL_CLOSED
```

The candidate can be configured, built and host-tested without hardware.
Physical GPIO, UART, CAN, motor and USB behavior remains outside this
candidate's accepted evidence.

## Presets

| Preset | Purpose | Physical behavior |
|---|---|---|
| `board-smoke` | minimal F407 software image | not verified |
| `can-rx` | passive CAN receive image | not verified |
| `motor-safe` | zero-by-default motor image | non-zero output not authorized |
| `board-smoke-release` | optimized smoke image | not verified |
| `usb-cdc` | fail-closed USB CDC image | controller stopped by default |
| `usb-cdc-release` | optimized fail-closed USB image | controller stopped by default |
| `host-tests` | native protocol/state tests | no hardware |

## Shared-module changes

Do not edit a shared submodule to create a candidate-only behavior. Record the
problem against the authoritative integration source, integrate and regress
the shared fix there, then request a new deterministic export with updated
gitlinks.

## Motor safety

The normal `motor-safe` preset leaves the non-zero compile gate off. A
non-zero test requires separate approval and all explicit parameters. Stop on
unexpected motion, bus errors, stale feedback, overcurrent or abnormal heat.

## USB safety

The candidate contains the accepted software closure but no authorized USB
identity or physical-host evidence. Do not enable identity confirmation,
enumerate, attach to a host or report CDC success under this Gate.

## Evidence

The machine-readable source and export identities are under `release/`.
Ignored build output and local logs are not release evidence.
