# M2006 slider task

- Objective: add a standalone F407 + PS2 image controlling one M2006 slider.
- Baseline: `e51a6f5`; branch `feat/m2006-slider-manual`.
- Controls: right-stick horizontal controls signed speed in manual mode; Circle selects automatic shuttle; Cross selects manual.
- Safety: invalid/lost PS2, unhealthy CAN, offline motor, invalid configuration, or unavailable/conflicting limits commands zero current.
- Hardware: photoelectric limits are not installed, so the runtime reports limits unavailable and automatic mode cannot move.
- Change: added a pure slider controller, one-motor ThreadX runtime, PS2/USART1
  vehicle config, motor ID `0x205`, and `f407-m2006-slider-debug` preset.
- Validation: focused slider controller and product-contract tests PASS; full
  Host 60/60 PASS; fresh F407
  build PASS at RAM 55,960 B and Flash 65,852 B. ELF SHA-256 is
  `0DADE51C9DD6FC5E68536C9AF36342C1028EBA3A749F185B3C1031AED075182D`.
- Flash: on 2026-09-05, the corrected ELF above was programmed through the Horco
  CMSIS-DAP v2 associated with COM12, verified OK, and the STM32F407 target was
  reset. Probe serial was `482752132243`; SWD speed was 1000 kHz.
- Recovery evidence: the first attempt stopped before Flash initialization at
  `CMD_INFO failed`. Restarting only the exact Horco composite USB device with
  elevated `pnputil` restored CMSIS-DAP communication; a read-only target probe
  then passed before programming.
- Build graph: PS2 source present, DR16 source absent; all four submodules remain
  at the baseline gitlink commits.
- Hardware diagnosis: the first image registered `0x201`, but live CAN1
  telemetry showed the only motor feedback stream at `0x205`; this left the
  motor offline and intentionally forced zero current. A product-contract test
  failed against `0x201`, then passed after the profile changed to CAN1/`0x205`.
  Post-flash read-back showed CAN1 active with zero errors and the motor online.
  The PS2 receiver then reported `remote_disconnected`, so motion remained
  safely inhibited pending controller reconnection and a centered-stick sample.
- Not validated: motor direction on the mechanism, limit polarity/placement,
  travel-end stopping, and powered hardware motion.
- Prohibited here: submodule edits, additional hardware operation, commit,
  push, or Vault write.
