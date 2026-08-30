# Proposed Vault Update: F407 Core Hardware Validation

This file is a proposal only. The Vault was not modified.

## 2026-08-31 vehicle publication proposal

- Record fast-forward publication of
  `shiuhou/pnx_newtemplate_F4:chassis_x_arm@9dac12a`, after the referenced
  F407 modules and parent commits were confirmed public.
- No force push, flash, or new hardware observation occurred. The Vault was
  not modified.

## 2026-08-30 vehicle synchronization proposal

Status: **LOCAL SOFTWARE PASS; CURRENT ELFS NOT FLASHED; NOT PUSHED.** This is
a proposal only; the Vault was not modified.

- Record local `chassis_x_arm@3d3189d`, synchronized from the approved
  pure-F407 baseline `feat/f407-ps2-validation@d92c9be`.
- Record dependency pins `pnx_bsp@ee59c97`, `pnx_devices@5c418a4`,
  `pnx_libs@e7c3e7a`, and `pnx_modules@dffaca9`; modules follows
  `fix/f407-ps2-stack` and excludes H7-only product closure.
- Record the final operator-provided `servo_pe14` seed/lower bound of
  `900/900 us`. Current source naming maps PE14 to the gripper/J4 and PE13 to
  J3; unlock does not itself enable PWM output.
- Record focused Host **6/6 PASS**, full Host **57/57 PASS**, and fresh
  combined/chassis/ARM Debug builds PASS. The resulting SHA-256 values are
  `B4420016AF0D953F155512D67E1BAFB5CEB8269318B381412062B9E49429F60E`,
  `A6A096A5FC0337E1D80A0C61F81AC296F55AE7596BE5663D4237BE865467547E`, and
  `2B59BC54E6B50E1F140C0FC882CE00B40E0C6CE4B05C423C1B9503CB43E163AF`.
- Keep these exact ELFs as `NOT_FLASHED`; prior attended combined/chassis
  observations apply only to the previously recorded artifact.
- No remote branch or Vault file was modified.

## 2026-08-06 combined chassis + ARM proposal

Status: **EXACT ELF PROGRAMMED; ATTENDED PRODUCT BEHAVIOR PENDING.** This is a
proposal only; the Vault was not modified.

- Record the `f407-mycar-combined-debug` closure on `chassis_x_arm`, with
  implementation commit `a446f41123c4d5745d7125921e4287868409862c`.
  It composes one DR16 service, the four-M2006 mecanum chassis, the J1 M2006,
  J2-J4 PWM servos, and one global fail-closed output arbiter.
- Record software evidence: Host **57/57 PASS**; combined, standalone chassis,
  and standalone ARM F407 Debug builds PASS. Combined RAM/Flash use is
  60,920/98,640 B.
- Record exact programmed ELF SHA-256
  `A3BCFED755A390CDB790B9BDDEDCC012CF8533DBB80ECD15F29B261170A54561`.
  The successful 1000 kHz OpenOCD run reported `Programming Finished`,
  `Verified OK`, and reset after an earlier 2000 kHz CMSIS-DAP pipe failure.
- On 2026-08-10, fresh Host **57/57 PASS** and a clean combined build
  reproduced the same artifact hash and memory use. No board or CMSIS-DAP was
  present, so no new programming or hardware behavior was observed.
- Keep all combined-image motion, mode-transition, hold, disarm, and
  remote-loss behavior as `NOT YET OBSERVED`. Do not infer combined hardware
  acceptance from the earlier standalone chassis and ARM observations.
- No Vault file was modified; repository publication is tracked by the
  `chassis_x_arm` Git history.

## 2026-08-04 MyCar Direct BSP publication proposal

Status: **READY FOR REPOSITORY REVIEW as a proposal only; do not ingest
automatically.** The Vault was not modified.

- Record the official BSP publication at
  `pnx_bsp/F4_version_bsp@ee59c97a852586eb7f6fb50b402f2fe9ed112cbc`.
- Record the validated parent code head at
  `shiuhou/pnx_newtemplate_F4:feat/chassis@c4d8ebcde95d18f7873fbca66b7635ee54b1f1df`.
  Its five atomic commits cover Direct BSP selection/gitlink, explicit config
  requirements, the six-preset CI matrix, reference-demo quarantine and
  ownership documentation. The published handoff is a docs-only successor
  that changes no code or submodule gitlink; use the branch HEAD for its final
  documentation SHA.
- Record that `pnx_template/F4_version` was verified unchanged at
  `9f8707280d56ae1c8f8061cb9ec12e75f527d1f8` immediately before parent push.
- Preserve the unchanged shared pins:
  `pnx_devices@2349cc108c9ed477ccdcd700e802ea888975cdfd`,
  `pnx_libs@e7c3e7a2b9d825586ab3e0c413877180c4295df8`, and
  `pnx_modules@8ba925b60b11fec511a57622c199b57bb23f8f4e`.
- Record fresh recursive-clone evidence: Host **44/44 PASS**; all six F407
  presets configure/build/link PASS with zero warning lines; source graph,
  CAN/USART init authority, single `app_start`, public BSP symbol uniqueness,
  HAL-free public headers, no backend forwarding and retired-selector
  fail-fast gates PASS; parent and all submodules remain clean.
- Record that a repository-wide scan contains five historical absolute
  Windows paths in pre-existing documentation, but this commit range adds
  zero such references and no absolute-path-only artifact.
- Keep all hardware revalidation, tag creation and Vault ingestion as
  `NOT_RUN`.
- Keep the live mixed feature worktree separate: it remains at
  `feat/chassis@ff1897e3bc890bf6078aca08bc2d85064f0d40ca` with independent
  chassis, MaixCam, ARM/PWM and dirty remoter work. It was not reset, cleaned,
  merged or rebased during publication.

The Vault was not modified.

## 2026-08-03 MyCar Direct BSP pre-publication proposal (superseded)

Status: **READY FOR REPOSITORY REVIEW as a proposal only; do not ingest
automatically.** The Vault was not modified.

- Record that the live `pnx_f4_mycar` worktree remains on
  `feat/chassis@ff1897e3bc890bf6078aca08bc2d85064f0d40ca` and intentionally
  combines existing chassis, MaixCam, ARM/PWM and DR16 feature work.
- Record the accepted source-ownership migration: F407 implementations now
  compile from `pnx_bsp/<peripheral>/src`; the parent Board BSP copies are
  removed; public callers continue to use the same HAL-free `bsp::*` API; no
  backend-forwarding framework was restored.
- Record the BSP state precisely: detached
  `da09febe8f5bfe66993010247d4d6731d0ca492b` plus local four-channel PWM
  changes. The parent gitlink still records `c097c5b...`, so publication and
  remote reproducibility remain `NOT_RUN` until BSP and parent commits are
  reviewed and pushed in dependency order.
- Preserve shared pins `pnx_devices@2349cc1`, `pnx_libs@e7c3e7a` and
  `pnx_modules@8ba925b`; the existing three-file DR16/remoter module diff is
  separate feature work and was not altered by the integration.
- Record software evidence: native Host **45/45 PASS**, MaixCam Host
  **29/29 PASS**, all six F407 presets configure/build/link with zero warning
  lines, and source graph, generated-init authority, public-symbol uniqueness,
  HAL-free-header and no-backend-forwarding checks pass.
- Record the review-driven PWM fail-closed correction: shortening the shared
  TIM1 period is rejected if any existing channel pulse/CCR exceeds the new
  period; both the Host behavior contract and F407 source contract were
  observed RED before the fix and GREEN after it.
- Keep commit, push, tag, fresh recursive clone, all physical hardware
  revalidation and Vault ingestion as `NOT_RUN`.

The Vault was not modified.

## 2026-07-31 MyCar chassis Tasks 1-6 software implementation proposal

Status: **READY FOR REVIEW as a proposal only; do not ingest automatically.**
The final safety corrections passed fresh software validation and independent
code re-review. Hardware acceptance remains gated and the Vault was not
modified.

Proposed durable project update after repository review:

- Record `pnx_f4_mycar` on `mycar/f4` with baseline HEAD
  `ed9a2271371c61001fd7440f413b542c2ba64218`; the vehicle implementation is
  present only as unstaged working-tree changes.
- Record the completed vehicle-local path: DR16 manual mapping, X-mecanum
  inverse kinematics, four explicit-dt PI loops, sticky safety policy and a
  MyCar-only ThreadX adapter for four static M2006s on CAN1 IDs
  `0x201..0x204`.
- Record the fail-closed runtime corrections: blocking topic reads are
  isolated in a separate ingest thread; remote freshness is bounded to 120
  ticks; snapshot copy precedes the current-time sample; the motor watchdog
  has an independent modulo-four phase; terminal CAN deltas and timing
  overruns latch; telemetry is copied coherently.
- Record the final safety closure: offline switch history cannot release the
  startup interlock; controller faults require a fresh online switch release;
  runtime-sticky faults inhibit the controller and reset PI; malformed manual
  input faults closed; and all overrun paths reset controller state.
- Record final software evidence: Host **41/41 PASS**; all six F407 presets
  clean-built warning-free; exactly one `app_start` per ELF; Core/USB/PWM
  graphs contain no MyCar/CAN/USART/DR16/M2006 closure; MyCar generated config
  and ELF contain the expected four-motor runtime.
- Record the six final ELF SHA-256 values and memory figures from the current
  `HANDOFF.md` section rather than duplicating them into a less reviewable
  summary.
- Record that the default MyCar configuration deliberately remains invalid
  and zero-only because geometry, wheel identity/direction, limits and gains
  have not been measured or authorized.
- Keep Task 7 physical measurements and every Task 8 flash/live/non-zero/
  ground acceptance stage as `NOT_RUN`. Keep WCET and stack high-water as
  `NOT_RUN`, and same-cycle CAN send acceptance as `UNKNOWN` because the
  pinned handler returns `void`.
- Record that Tasks 1-6 received independent review with no remaining
  Critical or Important issue. Do not call this an operational chassis or
  hardware-validated closed loop.
- Record the procedural scope event: a failed Windows exclusion displayed
  four line snippets from the excluded prototype once; those snippets were
  treated as tainted, not used, and the file was neither opened nor modified.
- No commit, stage, push, remote change, shared-submodule change, hardware
  operation or Vault write occurred in this task.

The Vault was not modified.

## 2026-07-30 latest pnx_template main alignment proposal

- Record a local-only comparison and alignment against
  `pnx_template/main@cf6577765358822a1bc57c1ea17fe65a795ceb62`.
- Record exact shared pins:
  `pnx_devices@2349cc108c9ed477ccdcd700e802ea888975cdfd`,
  `pnx_libs@e7c3e7a2b9d825586ab3e0c413877180c4295df8`, and
  `pnx_modules@8ba925b60b11fec511a57622c199b57bb23f8f4e`.
- Record that `pnx_bsp/main` was not adopted because it diverges from the
  Direct BSP branch and leaks Board/HAL dependencies into its USART public
  surface. The F407 branch instead adds only the board-neutral line
  configuration declaration and a direct F407 implementation.
- Record parent config compatibility for disabled-by-default PS2 symbols and
  LK8016/LK9025 model identifiers. PS2, BMI088, AHRS, and Tactical EKF remain
  outside every RC2 image graph.
- Record fresh software evidence: Host CTest **35/35 PASS**; all five F407
  presets PASS with zero warnings and unchanged RAM/Flash; source/symbol
  gates PASS; Arm syntax checks PASS for Direct USART and latest
  PS2/Remoter/DR16/VT03/Referee sources.
- Preserve the deferred BMI088 boundary: the latest Device source still
  requires a Board-owned `GYRO_INT_Pin`/EXTI mapping. No BMI088/AHRS build or
  runtime support is claimed.
- Record that official `pnx_template/F4_version@aafa57c7` already retains the
  same Feishu workflow as main, while H7 USART10/IOC, temporary IMU
  diagnostics, IDE metadata, and PS2 demo composition were intentionally not
  ported.
- Preserve local-only state: no commit, push, tag, remote change, hardware
  operation, or Vault write was performed.

The Vault was not modified.

## 2026-07-30 RC2 release-closure proposal

- Record local `pnx_bsp` candidate
  `4d3ce2abb3dee18ad551cb03428563b38e384050`, which removes the shared
  forwarding implementation/contracts while keeping MCU-neutral public
  headers. It is an atomic commit now remotely reachable through official
  `pnx_bsp/F4_version@c83e892d9ba76e2671e8d1c8fbc2939a7a77e9df`;
  the merge tip adds only the official Feishu workflow.
- Record the USB lifecycle contract: `init(ok)` accepts raw config/callback
  ownership, creates required ThreadX resources, and schedules async startup;
  `connected()` means CDC transport usable; startup failures become
  observable fault state; writes are bounded; same-config re-init is
  idempotent and incompatible ownership is rejected.
- Record fresh local evidence: all five retained F407 presets configure,
  compile, and link with zero warnings, exactly one `app_start`, and no
  duplicate strong `bsp::*`; native CTest is **33/33 PASS**; all three retired
  validation selectors fail fast.
- Record the corrected source-graph definition: generated Board source
  presence alone is not Direct-BSP closure leakage. Core links no optional
  CAN/USART/SPI/Flash BSP or consumer closure. CMake derives CAN/USART init
  guards from the existing embedded source list; all five RC2 images compile
  those values as false. Their fresh ELFs contain zero CAN1/CAN2 or
  USART1/3/6 init calls and zero matching init symbols, while generated
  `can.c`/`usart.c` remain dormant compiled capability source.
- Record the corrected repository boundary:
  `H7_REGRESSION=OUT_OF_SCOPE`. F407 and H7 use independent BSP
  implementations and release paths; prior cross-injection results are void
  as F407 RC2 findings.
- Preserve current pre-publication status:
  `H7_REGRESSION=OUT_OF_SCOPE`, `SOURCE_GRAPH_ISOLATION=PASS`,
  `PNX_BSP_PUSH=PASS`, `PARENT_PUSH=NOT_RUN`,
  `FRESH_RECURSIVE_CLONE=NOT_RUN`, `RC2_TAG=NOT_CREATED`,
  `HARDWARE_REVALIDATION=NOT_RUN`.
- Preserve deferred capability status:
  `FLASH_CAPABILITY=UNSUPPORTED_UNTIL_RESERVED_PARTITION`,
  `BMI088_AHRS_CAPABILITY=NOT_IN_RC2_PRODUCT_GRAPH`, and
  `USB_TYPED_ADAPTER=UNSUPPORTED_UNTIL_CONTRACT_FIX`.

The Vault was not modified.

## Pre-RC2 direct F407 BSP proposal — historical

This section records the earlier uncommitted working-tree and hardware
evidence. Its software counts, H7 wording, and publication state are
superseded by the current RC2 release-closure proposal above.

- Record the architecture decision: one HAL-free public BSP contract and one
  F407 source per peripheral that directly defines the public symbols. The
  pure `detail::backend_*` forwarding layer is removed; no H7 work is included.
- Record the reusable-layer boundary: Device and Module code may consume the
  public BSP contract but cannot see F407 HAL handles, pins, DMA or IRQ names.
- Record that `pnx_devices`, `pnx_libs` and `pnx_modules` returned to the exact
  `pnx_template` commits:
  `8a6783e63d77a15940aea8245bbe2eb13a2f2b11`,
  `55bd94060b7be562ce7a6773822a6a4d2bcab9c0`, and
  `a54c493020ba9bcd5b43b99e068a06cdda9dd018`.
- Record that BMI088, DBUS RX and CAN/M2006 validation closures and dependent
  claims were retired because the template gitlinks do not reproduce their
  former APIs.
- Record fresh local software evidence: 23/23 host tests PASS; Core
  Debug/Release, USB Debug/Release and PWM Debug builds PASS; F407 ARM syntax
  checks for dormant CAN/SPI/Flash/USART direct sources and representative
  template Motor/Remoter consumers PASS; source graphs contain no former
  backend directory or retired validation source.
- Record that board-private host-tested policy now publishes USART RX metadata
  before enabling reception and serializes USB connection/TX lifecycle with a
  generation check so callbacks from retired connections are ignored.
- Record the authorized short Core hardware result: clean-first Core Debug
  ELF SHA-256
  `EDB72454568B5C719C622B5492BDCDF0B9AB61D1792343FD04D630E7A64A78CA`
  programmed and verified successfully; reset reached `app_start` twice;
  ThreadX PendSV execution, advancing DWT, and PH11 GPIO state activity were
  observed through SWD.
- Record the authorized optional hardware evidence:
  - PWM/servo machine evidence passed 5/5 bounded steps and disabled TIM1
    output with no fault.
  - Current direct CAN plus unchanged template Motor/DJI Device sources
    received ID `0x203`, completed the guarded `+500`/125-cycle pulse, returned
    current to zero, and reported no CAN error/bus-off/drop/fault.
  - A user-authorized five-second M2006 follow-up retained `+500` and measured
    exactly 5000 ThreadX ticks at 1000 Hz. It received 7712 CAN frames,
    completed 2501 guarded control cycles, returned current and final speed to
    zero, and reported no CAN error/bus-off/drop/fault, ESR, crash, or harness
    fault. Physical rotation and temperature require user observation.
  - Current direct SPI plus unchanged template BMI088 Device polling passed
    chip-ID self-test, accel/gyro reads, changing samples, and valid
    `32.875 C` temperature with the heater disabled.
  - Preserve the BMI088 portability limitation: template `bmi088.cpp`
    references `GYRO_INT_Pin`, which this IOC does not define. The disposable
    polling build used inert `GYRO_INT_Pin=0U`; DRDY was not exercised.
  - Current direct USART plus unchanged template Msg/DR16 sources received
    and parsed live 18-byte DBUS frames. The decisive run reported RX/update
    `310/310`, errors/busy `0/0`, `offline=0`, 272 motion samples, and non-zero
    decoded axes. The unchanged 768-byte DR16 stack showed no crash/fault.
  - Preserve the DBUS limitation: one intervening run observed 225
    short-frame/error/busy events and went offline; repeating the exact
    artifact was healthy. Cable placement and signal quality need attended
    follow-up.
  - Physical actuator motion was not observed by Codex.
- Preserve the evidence boundary:
  `CORE_HARDWARE=PASS_SHORT_DEBUGGER_OBSERVED`,
  `PHYSICAL_LED=NOT_OBSERVED`, `LONG_SOAK=NOT_RUN`,
  `USB_HARDWARE=NOT_RUN_FAIL_CLOSED_UNASSIGNED_IDENTITY`,
  `SERVO_MACHINE_EVIDENCE=PASS`, `CAN_M2006_MACHINE_EVIDENCE=PASS`,
  `BMI088_POLLING=PASS_WITH_DRDY_LIMITATION`,
  `PHYSICAL_ACTUATOR_MOTION=NOT_OBSERVED_BY_CODEX`,
  `DBUS=PASS_WITH_INTERMITTENT_SIGNAL_OBSERVATION`, `H7=NOT_RUN`,
  `REMOTE_CI=NOT_RUN`, `FRESH_RECURSIVE_CLONE=NOT_RUN`.
- Record that the repository working tree is uncommitted and unpushed; publish
  `pnx_bsp` before the parent gitlink, then verify a recursive fresh clone.

The Vault was not modified.

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

## 2026-07-29 release-hardening proposal delta

- Record the corrected release state: attended DBUS live-frame validation is
  PASS, superseding DBUS software-only language in current release guidance.
- Record the ThreadX lifecycle rule: the F407 composition root runs in
  `tx_application_define()` before a schedulable application thread exists;
  waiting ThreadX APIs belong in thread entries.
- Record the USART diagnostic semantic fix: a circular-DMA restart returning
  `busy` increments `busy_count` rather than falsely reporting an error. A
  host regression test was observed RED before the change and PASS after it.
- Record the minimal CI gate proposal: recursive submodule checkout, host
  CTest, F407 Core build, and F407 DBUS build. It remains unexecuted remotely
  until the corresponding parent and submodule commits are pushed.
- Fresh local evidence after these changes: seven normal embedded builds,
  isolated CAN build, and 25/25 host tests PASS. No Vault content was
  modified.

## 2026-07-29 DBUS live-frame proposal delta

- Supersede the former `DBUS_LIVE_FRAME=NOT_RUN` boundary: attended F407
  validation is now PASS. USART3 PC11 DMA received 18-byte frames, the DR16
  parser published live data, and observed stick movement changed decoded
  axes.
- Record the two narrow source corrections needed by the live test: DR16
  ThreadX stack 768 -> 1536 bytes in `pnx_modules`, and DBUS demo subscription
  moved from `tx_application_define()` context into its monitor thread.
- Record software evidence for the corrected tree: DBUS embedded build PASS
  and 25/25 host tests PASS.
- Record safe restoration to Core artifact SHA-256
  `78F5C5C7B8A9E920370EAE43973F913988F1B6838EE81594C945FB78497A5463`, with
  OpenOCD program/verify PASS.
- No Vault content was modified. These changes remain uncommitted and were
  not pushed by this validation task.

## 2026-07-29 team-internal documentation and local revalidation proposal

- Record that the team-internal RC README now defines the backend, closure,
  and fail-closed terms and documents the repository tree plus CMake source
  composition flow.
- Record the fresh local software evidence on parent
  `7f0129ad6a789e4cb466a51697d0bad2a10854d1` plus document-only changes:
  seven normal embedded presets and isolated CAN/M2006 built successfully;
  native host tests passed 25/25.
- Record that the Core compile graph excludes all optional backend/closure
  sources and the shared-layer leakage scan remained clean.
- Retain the existing evidence boundary: no hardware was operated in this
  pass; `DBUS_LIVE_FRAME=NOT_RUN`, long soak, concurrent runtime, and
  destructive Flash validation remain unverified.
- `testing.md` is a transient checklist outside this repository; `HANDOFF.md`
  remains the detailed evidence record.

The Vault was not modified.

## 2026-07-29 CI release-gate proposal

- Record parent commit `648da10df13acd93b16053eb2d2921539b8fe7a3` as the
  minimal GitHub Actions authentication correction for private recursive
  submodule checkout; it injects only a repository-managed read-only secret
  into `actions/checkout@v7`.
- Record GitHub Actions run `30448274030` as remote evidence that recursive
  submodule checkout, host CTest, F407 Core build, and F407 DBUS build all
  succeeded from the pushed branch.
- Do not record the CI secret value. The remaining non-code release decisions
  are licensing and authorized production USB identity.

The Vault was not modified.

## 2026-07-29 final remote-gate revalidation proposal

- Record a second independent `--recurse-submodules` checkout of remote
  parent `957ce253a44d229b1aced496e8381956ace57bf0`.
- Record that it retrieved the four published F407 submodule commits, built
  all seven named F407 presets plus the isolated attended CAN image, and
  passed 25/25 host tests using root CMake with `PNX_HOST_TESTS=ON`.
- No hardware was operated. The DBUS live-frame and destructive Flash hardware
  limitations remain unchanged.

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

## 2026-07-29 final release-candidate reproducibility proposal

- Supersede earlier no-push statements: four submodule branches and parent
  `refactor/f407-minimal-architecture` are now pushed.
- The parent records cloneable `shiuhou` HTTPS URLs for all submodules.
- A clean recursive checkout of parent
  `4a3a101a3d201b3d61cad71df730ed059559b7e0` reached all exact gitlinks,
  built seven normal F407 images plus isolated CAN, and passed 25/25 host
  tests.
- Retain the hardware boundary: this release cleanup did not operate hardware;
  DBUS live frames and destructive Flash hardware remain unverified.

The Vault was not modified.

## 2026-07-31 MyCar chassis planning proposal

Proposed durable project update after repository review:

- Record that the vehicle worktree is `pnx_f4_mycar`, branch `mycar/f4`, with
  planning baseline `ed9a2271371c61001fd7440f413b542c2ba64218`.
- Record the user-approved v1 scope: DR16 manual control, four M2006s, mecanum
  inverse kinematics and four wheel-speed PI loops.
- Record the ownership decision: vehicle code under `vehicle/chassis`, no
  `pnx_application`, and no shared submodule/API change.
- Record the fail-closed policy: unmeasured geometry, directions, speed limits,
  gains and non-zero current limits cannot enable output.
- Record the baseline limitation: car-worktree submodules were uninitialized;
  Host configure passed but compilation failed on missing shared headers.
- Keep build, flash, runtime, hardware motion and repeated acceptance as
  unverified until the implementation plan produces fresh evidence.

No Vault file was modified by this repository task.
