# Proposed Vault Update: F407 Core Hardware Validation

This file is a proposal only. The Vault was not modified.

## Project-state proposal

- Repository:
  `C:\Users\USER\Desktop\RM\rm校內賽\2026\firmware\pnx_f4_minimal`
- Branch: `refactor/f407-minimal-architecture`
- Firmware commit: `342efc481b7153acc2815ed511139a6c8847ff66`
- Core Debug hardware status: `VERIFIED` for the shortened attended smoke
  scope.
- Long-duration Core status: `UNVERIFIED`; the 30-minute soak was not run.
- USB CDC status: `VERIFIED` for the authorized shortened F407 ZLP boundary
  and recovery scope.
- CAN, motor-control, servo PWM, IMU, UART/DBUS, and concurrent-runtime
  hardware status: `UNVERIFIED`.

## Verified experiment proposal

- STM32F407 was programmed and Flash verification passed.
- Observed startup chain:
  `Reset_Handler -> main -> tx_application_define -> app_start`.
- ThreadX reached and remained in its scheduler wait path during samples.
- DWT was enabled and increased across steady-state samples.
- Reset recovery returned to ThreadX with DWT enabled.
- PH11 green-indicator GPIO transitioned, and the user observed the green LED
  blinking.
- The connected M2006 and A2 servo did not move.
- Evidence and limitations are recorded in `HANDOFF.md`.

## Open questions

- Does the Core image remain stable for the full planned 30-minute soak?
- Does the separately authorized USB identity enumerate and recover correctly?
- Do CAN, motor, servo, IMU, and UART vertical slices work on hardware?

## Reusable workflow note

For this workstation and probe, use Horco CMSIS-DAP v2 with OpenOCD
`interface/cmsis-dap.cfg`, not `interface/stlink.cfg`. GNU GDB 13.3 has a
non-ASCII Windows path issue for this repository; an ASCII-path copy of the
ELF may be used only after verifying that its SHA-256 matches the source
artifact.

## USB CDC follow-up proposal

- The separately authorized RM26_F4 identity enumerated on COM17 as
  `VID_0483&PID_5710`, product `Pikachu`, serial `LXYCAT`.
- USB CDC echo passed at 1, 16, and 63 bytes.
- The exact 64-byte echo failed with zero bytes returned before timeout.
- CPU fault registers remained clear and ThreadX was still scheduled after the
  failure.
- Read-only comparison found that the F407 USBX 6.1.10 callback bulk-IN path
  lacks the automatic ZLP support enabled in the `pnx_template` USBX 6.4.0
  configuration. This is the leading explanation, not yet a verified fix.
- USB CDC hardware status is `PARTIAL PASS / BLOCKED`; later binary, packet
  sequence, reconnect, reset/re-enumeration, and sustained-runtime checks are
  `NOT RUN`.
- The repository has an authorized uncommitted `CMakeLists.txt` change
  removing the old `0x0483` rejection. No firmware fix, commit, push, tag,
  IOC regeneration, or submodule change was made.

The preceding paragraph is retained as the pre-fix experiment. It was
superseded by the separately authorized `P0-F407-USB-ZLP-BOUNDARY-FIX`:

- Root cause was confirmed as missing termination for an exact 64-byte CDC IN
  packet in the F407 USBX 6.1.10 callback path.
- A board-local two-phase completion sends one ZLP after a successful exact
  full-packet transfer; common BSP and vendor/generated USBX remain unchanged.
- The ZLP policy was developed with RED/GREEN host regressions, including the
  partial-completion error path.
- Final software evidence: 12/12 host tests and 4/4 embedded builds PASS,
  compiler/linker warnings zero.
- Final programmed artifact SHA-256:
  `67E0989FB74EE5B0AC3B6C37550E52072892BE57198AE60BC0DF92D82F64B3EC`.
- Final artifact passed COM17 enumeration, 64-byte binary echo, 20/20 mixed
  1/16/63/64-byte packets, software reset/re-enumeration, and post-reset
  64-byte echo.
- The immediately preceding artifact also passed 100/100 mixed packets,
  physical USB cold reconnect, and a 15.005-second 15,909-packet continuous
  full-packet echo.
- The Horco CMSIS-DAP physical SRST command failed once. The target remained
  reachable, and using the configured Cortex-M `SYSRESETREQ` path with
  `reset_config none` allowed verified programming and reset.
- No IOC, generated/vendor USBX, common USB API, preset, submodule, commit,
  push, tag, or remote change was made.

## 2026-07-28 CAN1/M2006 proposal delta

- Record the clean F407 CAN1 vertical slice as hardware PASS:
  F407 board bxCAN backend -> existing `pnx_bsp/can` -> existing M2006/device
  handler -> dedicated application.
- Record final CAN artifact SHA-256
  `02C02F6A2B106B51F96E6A871DEFCC7DBAFE80747B6E44BC2C7AEB386BD3591D`.
- Record final telemetry: CAN RX/TX `9310/4583`, last ID `0x203`,
  error/drop/fault epoch `0/0/0`, pulse cycles `125`, final current `0`,
  complete/faulted `1/0`.
- Record the operator-observed very small, brief movement on the immediately
  preceding artifact using the same `+500`/250 ms sequence.
- Record software evidence: 18/18 host tests, backend source acceptance, and
  Core/USB/CAN Debug builds PASS with zero build warnings.
- Record that Core and USB product graphs exclude the CAN/M2006 closure.
- Record H7 leakage removal:
  `bsp::can::bus::fdcan1` -> `bsp::can::bus::none`.
- Record the provenance boundary: `pnx_devices` remains at detached HEAD
  `8a6783e63d77a15940aea8245bbe2eb13a2f2b11`; the one-line leakage correction
  is uncommitted inside the submodule, and the parent pointer was not changed.
- Record safe restoration to Core SHA-256
  `0B34622FE890F2BDF665F26D1CEF5120D139A6D365F66CAEF6A11993C2F529F2`.
- A2 PWM was not initialized or exercised.

The Vault was not modified.

## 2026-07-29 release-candidate cleanup proposal delta

- Record the F407 release-hygiene cleanup: unused H7-template SPI2/SPI6 and
  TIM3/TIM12 generated configuration was removed; default Core no longer
  links the unselected Flash closure; and normal presets explicitly reset all
  optional application selectors.
- Record documentation authority: root README for image/build/release use;
  board README for IOC/manual-resource ownership; AGENTS for architecture
  constraints.
- Record fresh software evidence: 7/7 normal embedded presets, one isolated
  CAN attended image, and 25/25 host tests PASS; normal Core Ninja graph has
  no optional closure source.
- Preserve the evidence boundary: no hardware was operated in this cleanup;
  `DBUS_LIVE_FRAME=NOT_RUN`; Flash hardware is unverified; public-release
  licensing and remote fresh-clone verification remain open.
- Record the release-provenance repair: `.gitmodules` now names the tested
  `shiuhou` HTTPS remotes for `pnx_devices`, `pnx_libs`, and `pnx_modules`;
  the previous HKUSTGZ SSH URLs were not cloneable by the release environment.

The Vault was not modified.

## 2026-07-28 shared-peripheral completion proposal delta

- Record the F407 portable boundary cleanup as complete for SPI, PWM, Flash,
  memory placement, BMI088, USART3/DBUS software, and the motor default bus.
- Record formal local submodule commits:
  `pnx_bsp@899c0b491ec81e48ba8ed66ae90d451fb2bddc1b`,
  `pnx_devices@e90601d8676922db954c90a06de97ef82d87bf01`,
  `pnx_libs@205217a42ac4e1e556e63f594eb7671f47e0e2b9`, and
  `pnx_modules@deaae6720a6051c0276b1ffbeb3fc0fc875456dd`.
- Record final software evidence: seven normal embedded presets PASS, the
  explicit attended CAN validation build PASS, 25/25 host tests PASS, and
  zero observed build warnings.
- Record final BMI088 hardware artifact SHA-256
  `F7E32A275CDE72D309700A97285DDAB1557A777BE3E524A35F0AFA4FF2535C02`.
  It returned IDs `0x1E/0x0F`, incrementing samples, changing six-axis raw
  data, and complete/faulted `1/0`.
- Record that the automatic M2006 pulse image was removed from normal presets
  and retained only behind the explicit attended-validation option.
- Record `DBUS_LIVE_FRAME=NOT_RUN` by operator direction. USART contract,
  DR16 parser, remote module, and F407 DBUS image build are verified software
  evidence only.
- Record final safe restoration to Core artifact SHA-256
  `4736B2E1B0EFC47CD2A3D94CB10112EBA362DC57FADF38188BAB3B5A8C427BE1`,
  with program/verify PASS, ThreadX execution, DWT enabled/incrementing, and
  GPIOH ODR `0x800`.
- No submodule branch or parent branch was pushed. No Vault content was
  modified by this task.

The Vault was not modified.

## 2026-07-28 PWM/A2 proposal delta

- Record the F407 servo vertical slice as hardware PASS:
  board-neutral PWM contract -> F407 TIM1/PE11 backend -> C-board C2 signal
  mapping -> dedicated bounded servo application.
- Record the exact sequence
  `1500 -> 1450 -> 1500 -> 1550 -> 1500 us`, followed by center and output
  disable.
- Record operator-observed servo movement and final telemetry:
  heartbeat/steps `5/5`, pulse `0`, complete/faulted `1/0`, output enabled
  `0`.
- Record software evidence: 20/20 host tests and Core/USB/CAN/PWM Debug builds
  PASS with zero observed build warnings.
- Record PWM artifact SHA-256
  `C3BF015E8354CD31369A031419CB676CFA33D92AAF425E32B61FE893D0E5EC5A`.
- Record that Core, USB, and CAN graphs exclude the PWM backend.
- Record safe restoration to Core SHA-256
  `6D0CEFDCD1CE31DD4E0074813B4596912B2AFE6B4D6E278B0AE97EC83352964C`.
- Record the provenance boundary: `pnx_bsp` remains at detached HEAD
  `f61e8ac8ac4b75d93b021ec32a8ecf56fc36a73b`; its PWM change is uncommitted.
- Record the deferred integration boundary: dormant BMI088 code still uses
  the former timer-specific channel names and must migrate during its own
  vertical slice.

The Vault was not modified.
