# F407-only Team Repository Rules

## Repository role

This is a derived, unpublished DJI C-board F407 candidate. It is not the
authority for shared PnX interfaces, devices, modules or libraries.

```text
F407_ONLY_HARDWARE=HARDWARE_UNVERIFIED
F407_ONLY_TEAM_RELEASE=NOT_PUBLISHED
USB_DESCRIPTOR_IDENTITY=UNASSIGNED_FAIL_CLOSED
```

## Required boundaries

- Keep `pnx_bsp`, `pnx_devices`, `pnx_libs` and `pnx_modules` at the exact
  recorded gitlinks.
- Land shared fixes in the authoritative multi-board repository, then export a
  new candidate.
- Keep IOC, startup, linker, HAL, IRQ, DMA, memory and USB backend changes
  board-specific.
- Do not treat build/static results as physical runtime evidence.
- Preserve zero-by-default motor behavior and the explicit non-zero arm Gate.
- Keep USB descriptor identity fail-closed until an authorized identity is
  separately approved.
- Do not push, publish, tag, change remotes, connect hardware or flash a board
  without task-specific authorization.

## Verification

Use the documented presets and run the candidate checker, CubeMX production
Gate and all host tests before handing changes back for integration.
