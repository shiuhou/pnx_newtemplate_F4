# Current task

- Objective: add a standalone F407 + PS2 image for one M2006 friction-wheel
  slider, with right-stick manual motion and fail-closed future auto shuttle.
- Branch/baseline: `feat/m2006-slider-manual` from committed `e51a6f5`.
- State: software implementation complete in the uncommitted worktree; the
  exact verified ELF was programmed to the connected F407 on 2026-09-05.
- Controls: default manual after a centered sample; right-stick horizontal
  drives both directions; Circle selects automatic; Cross selects manual.
- Safety: PS2/CAN/motor/invalid-input failure sends zero. Automatic mode sends
  zero because the photoelectric limit binding deliberately reports not ready.
- Hardware binding: runtime CAN telemetry showed the connected single motor
  continuously reports feedback ID `0x205`, so the slider profile now binds
  CAN1/`0x205` and the DJI handler sends its command through frame `0x1FF`.
- Verified: Host 60/60 PASS; fresh `f407-m2006-slider-debug` build PASS at RAM
  55,960 B and Flash 65,852 B; corrected ELF SHA-256 `0DADE51C...75182D`;
  OpenOCD program/verify/reset PASS through Horco CMSIS-DAP serial
  `482752132243`.
- Runtime read-back: CAN1 active with zero errors, `last_id=0x205`, motor
  online with advancing feedback count. PS2 receiver currently reports
  `remote_disconnected`, so the safety gate correctly keeps current at zero.
- Next: attended low-current hardware direction test, then bind and validate
  both photoelectric limits before enabling automatic motion.
- Prohibited: push, commit, submodule edit, Vault write, or additional hardware
  operation without explicit authorization.
