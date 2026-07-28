# F407 Core Hardware Validation Handoff

## Context

- Date: 2026-07-28
- Repository: `C:\Users\USER\Desktop\RM\rm校內賽\2026\firmware\pnx_f4_minimal`
- Branch: `refactor/f407-minimal-architecture`
- Firmware commit: `342efc481b7153acc2815ed511139a6c8847ff66`
- Firmware artifact: `build/f407-debug/pnx_embedded.elf`
- Artifact SHA-256: `78C50EBFEE979EAD1332309FEBB7E74232310EA94F0B45C58CBCE34925688DF5`
- Operator: user, attended
- Pre-existing local item: untracked `testing.md`

## Objective

Validate the F407 Core Debug image on the DJI C-board without changing the
frozen P0 firmware baseline or exercising motor, servo, CAN, USB, IMU, or
other peripheral closures.

## Actual changes

No firmware, configuration, IOC, generated source, preset, submodule, or Git
history changes were made.

## Hardware and tools

- Target detected as STM32F407, Cortex-M4 r0p1, 1024 KiB Flash.
- Probe detected as Horco CMSIS-DAP v2, serial `482752132243`.
- OpenOCD 0.12.0 used with `interface/cmsis-dap.cfg` and
  `target/stm32f4x.cfg`.
- Arm GNU GDB from toolchain 13.3.Rel1 was used for breakpoint and register
  observations.
- An M2006 on CAN1 controller ID 3 and an A2 servo were physically connected,
  but the Core image sent no motor command and enabled no servo PWM.

## Verified evidence

- Flash programming completed and OpenOCD reported `Verified OK`.
- Reset PC was `0x08006214`, symbol `Reset_Handler`.
- Hardware breakpoints observed, in startup order:
  - `main`
  - `tx_application_define`
  - `app_start`
- Steady-state PC samples were in ThreadX `__tx_ts_wait`.
- DWT was enabled with `DWT_CTRL=0x40000001`.
- Steady DWT samples increased:
  - `0x5D564709 -> 0x675F0987`
  - final samples `0x2F06B20A -> 0x362A5DA1 -> 0x3C969AE7`
- After `reset run`, the target returned to ThreadX `__tx_ts_wait`; DWT was
  enabled again.
- GPIOH ODR changed from `0x00000000` to `0x00000800`, directly observing the
  PH11 green-indicator output transition.
- User observation:
  - `GREEN_LED=BLINKING`
  - `M2006=NO_MOVEMENT`
  - `A2_SERVO=NO_MOVEMENT`
- No HardFault, halt, unexpected reset, motor movement, or servo movement was
  observed during the attended smoke interval.
- OpenOCD was shut down after explicitly resuming the target; the MCU was left
  running independently.

## Failed attempts and resolution

- The initial OpenOCD attempt used `interface/stlink.cfg` and failed at probe
  open. Windows and pyOCD enumeration showed that the connected probe was
  CMSIS-DAP, not ST-Link. Using `interface/cmsis-dap.cfg` resolved the issue.
- GDB could not reopen the ELF through the repository's non-ASCII path. The
  ELF was copied to an ASCII temporary path and its SHA-256 was verified equal
  before use.
- A combined breakpoint run exceeded the F407 hardware-breakpoint budget
  because `HardFault_Handler` resolved to two locations. The startup points
  were then observed with a smaller sequential breakpoint set.

## Result

`ATTENDED-F407-CORE-HARDWARE-VALIDATION`: **PASS for the shortened attended
Core smoke scope**.

The planned 30-minute soak was shortened by user direction. Approximately a
few minutes of runtime plus a 90-second uninterrupted soak were observed.
Therefore the 30-minute stability criterion remains **NOT RUN** and must not
be cited as verified.

## Remaining scope

- Core 30-minute stability: `UNVERIFIED`
- USB CDC hardware: `UNVERIFIED`
- CAN RX/TX and motor control: `UNVERIFIED`
- Servo PWM: `UNVERIFIED`
- IMU, UART/DBUS, and concurrent-peripheral runtime: `UNVERIFIED`

## Rollback point

Git HEAD remains
`342efc481b7153acc2815ed511139a6c8847ff66`; the authorized USB changes are
uncommitted working-tree changes.

## USB CDC hardware follow-up

### Authorized configuration change

- The user explicitly authorized removal of the CMake rejection of VID
  `0x0483` and use of the existing RM26_F4 USB identity.
- At the initial identity-validation stage, the only production-source
  working-tree change was removal of that rejection block from
  `CMakeLists.txt`. The later separately authorized ZLP fix is recorded below.
- Configured identity:
  - VID `0x0483`
  - PID `0x5710`
  - manufacturer `STMicroelectronics`
  - product `Pikachu`
  - serial `LXYCAT`
- No IOC, generated source, preset, submodule, commit, remote, or tag was
  changed.

### Build and programming evidence

- Preset: `f407-usb-cdc-debug`
- Build: PASS with zero compiler/linker warnings.
- Pre-fix artifact: `build/f407-usb-cdc-debug/pnx_embedded.elf`
- Pre-fix artifact SHA-256:
  `05DD0BE20E8AC9C3D54E2E5D199D0958214ADCCCBE4B8DCEF5CF9BE6AB54D419`
- OpenOCD programming and verify: PASS.
- The target continued running ThreadX with DWT increasing and the PH11
  heartbeat active before the USB data cable was inserted.

### Host and CDC evidence

- Horco probe CDC enumerated as COM12.
- Target enumerated as COM17 with hardware ID
  `USB\VID_0483&PID_5710\LXYCAT`, product `Pikachu`, and repeated PnP status
  `OK`.
- CDC echo:
  - 1 byte: PASS
  - 16 bytes: PASS
  - 63 bytes: PASS
  - 64 bytes: FAIL; 64 bytes sent, zero bytes returned before host timeout
- Binary payload, 100-packet sequence, reconnect, reset/re-enumeration, and
  sustained-runtime tests were not run after the first boundary failure.
- Fault-register inspection after the failure showed CFSR, HFSR, DFSR, and
  AFSR all zero; the PC was in ThreadX `__tx_ts_wait`.

### Read-only diagnosis

- F407 carries USBX 6.1.10; its callback bulk-IN path sends an exact
  max-packet-size payload with equal transfer and host lengths and has no
  automatic ZLP support.
- `pnx_template` carries USBX 6.4.0, defines
  `UX_DEVICE_CLASS_CDC_ACM_WRITE_AUTO_ZLP`, and its callback bulk-IN path asks
  the stack to append a ZLP at this boundary.
- The observed 1/16/63-byte pass and exact 64-byte timeout are therefore
  consistent with a missing terminating ZLP. This is a strong causal
  inference, not yet a verified fix.

### Pre-fix USB result

`ATTENDED-F407-USB-CDC-HARDWARE-VALIDATION`: **PARTIAL PASS / BLOCKED**.

Enumeration and short CDC RX/TX echo passed. The required 64-byte boundary did
not pass, so USB CDC is not accepted and the CAN vertical slice must not be
started from this firmware state. No automatic source fix was made.

## P0 F407 USB ZLP boundary fix

### Authorization and scope

- The user explicitly authorized `P0-F407-USB-ZLP-BOUNDARY-FIX` and attended
  hardware revalidation.
- Production changes:
  - `boards/dji_c_board_f407/pnx_backends/usb_tx_completion.hpp`
  - `boards/dji_c_board_f407/pnx_backends/usb_backend.cpp`
- Test changes:
  - `tests/host/CMakeLists.txt`
  - `tests/host/usb_contract_host_tests.cpp`
- The fix adds a board-local asynchronous completion phase. An exact,
  successfully completed 64-byte CDC IN payload schedules one zero-length
  callback write before the common BSP receives the final completion.
- IOC, generated code, vendor USBX, common USB API, presets, submodules, CAN,
  motor, servo, IMU, commit history, remotes, and tags were not changed.

### TDD evidence

- Initial regression build failed because the completion policy did not
  exist.
- A deliberately non-ZLP scaffold then built, and
  `usb_f407_zlp_boundary` failed on the expected 64-byte action assertion.
- After implementation, the focused test passed.
- A second regression for a partial 64-byte completion failed before the
  guard and passed after requiring `actual == requested` before ZLP.
- Final host result: 12/12 PASS.

### Build evidence

- Fresh configure and build passed for:
  - `f407-debug`
  - `f407-release`
  - `f407-usb-cdc-debug`
  - `f407-usb-cdc-release`
- Compiler/linker warnings: zero.
- The Core source graph contains no `usb_backend.cpp` compile entry.
- Final artifact:
  `build/f407-usb-cdc-debug/pnx_embedded.elf`
- Final size: 2,281,708 bytes.
- Final SHA-256:
  `67E0989FB74EE5B0AC3B6C37550E52072892BE57198AE60BC0DF92D82F64B3EC`
- Final OpenOCD programming and verify: PASS.

### Hardware validation

- Final artifact enumeration:
  `USB\VID_0483&PID_5710\LXYCAT`, COM17, PnP status `OK`.
- Final artifact:
  - 64-byte binary echo: PASS.
  - 20/20 mixed echo sequence using 1/16/63/64-byte payloads: PASS.
  - SWD `SYSRESETREQ`, re-enumeration, and post-reset 64-byte echo: PASS.
- The immediately preceding implementation artifact additionally passed:
  - 1/16/63/64-byte boundary echo.
  - 64-byte binary echo.
  - 100/100 mixed packets, including 25 exact 64-byte packets.
  - Physical USB power/data cold disconnect and reconnect followed by a
    64-byte echo.
  - 15.005 seconds of continuous 64-byte bidirectional echo:
    15,909 packets and 1,018,176 bytes in each direction.
- The final source change after those extended checks only guards the
  partial-success error path; the final artifact was rebuilt, programmed,
  verified, and rechecked as listed above.

### Hardware/tool issue

- The first final-programming attempt reached the F407 but the Horco probe
  failed its physical SRST command with
  `CMD_DAP_SWJ_PINS failed / Unable to reset target`.
- A diagnostic attach with `reset_config none` read PC `0x0800F8BC`, proving
  SWD and target power were valid.
- Programming with physical SRST disabled and the STM32F4 target's Cortex-M
  `SYSRESETREQ` path completed and verified successfully.
- OpenOCD was shut down; final process count was zero.

### Final USB result

`P0-F407-USB-ZLP-BOUNDARY-FIX`: **PASS in the authorized shortened scope**.

The original exact-64-byte failure is reproduced, explained, regression
tested, and verified fixed on hardware. USB CDC no longer blocks beginning a
separately authorized CAN vertical slice. No commit or push was performed.

## F407 CAN1/M2006 vertical slice

Status: **PASS in the authorized attended scope**.

### Architecture changes

- Added the board-local STM32F407 bxCAN backend:
  `boards/dji_c_board_f407/pnx_backends/can_backend.cpp`.
- Reused the existing `pnx_bsp/can` contract without modifying it.
- Added the dedicated `PNX_ENABLE_CAN_M2006` application closure. Core and USB
  Debug Ninja graphs contain none of the CAN backend, CAN BSP, DJI motor, or
  CAN/M2006 application sources.
- The application uses the existing `m2006` and `djimotorhandler` classes
  directly. It does not restore `dji_motor_service`, an arm registry/token,
  USB arm commands, a factory, or a manager.
- Removed the remaining H7-specific default
  `bsp::can::bus::fdcan1` from
  `pnx_devices/motors/motor/include/motor.hpp`; the default is now the
  MCU-neutral `bsp::can::bus::none`.
- `pnx_devices` is a detached-HEAD submodule at
  `8a6783e63d77a15940aea8245bbe2eb13a2f2b11`. The one-line `fdcan1` correction
  is an uncommitted submodule working-tree modification; no submodule pointer
  was changed.
- A2 PWM was not initialized or enabled.

### Software validation

- Host tests: 18/18 PASS:
  - USB contract: 12.
  - CAN contract: 4.
  - M2006 template path: 1.
  - bounded one-shot sequence: 1.
- F407 CAN backend source acceptance: PASS.
- Embedded Debug builds:
  - Core: PASS, CAN/M2006 closure absent.
  - USB CDC: PASS, CAN/M2006 closure absent.
  - CAN/M2006: PASS, compiler/linker warnings zero.
- Non-generated/non-vendor source search found zero
  `bsp::can::bus::fdcan1`/`fdcan1` hits.

### Attended hardware validation

- Final CAN/M2006 ELF:
  `build/f407-can-m2006-debug/pnx_embedded.elf`
- Size: 1,572,932 bytes.
- SHA-256:
  `02C02F6A2B106B51F96E6A871DEFCC7DBAFE80747B6E44BC2C7AEB386BD3591D`
- OpenOCD programming and verify: PASS.
- First non-intrusive final telemetry snapshot:
  - heartbeat: 4,583
  - CAN RX: 9,310
  - CAN TX: 4,583
  - last ID: `0x203`
  - error/drop/fault epoch: `0/0/0`
  - non-zero pulse cycles: 125
  - final commanded current: 0
  - complete/faulted: `1/0`
- The operator observed one very small, brief M2006 movement on the immediately
  preceding implementation artifact using the same `+500`/250 ms sequence.
- An earlier symbol-read attempt halted the target and reproduced the known
  debugger-induced FIFO overflow. That observation was discarded. The final
  telemetry above was captured on the first halt after the final run and had
  zero CAN error/drop/fault.

### Safe restoration

- The board was restored to the normal Core Debug image.
- Restored Core ELF size: 1,418,560 bytes.
- Restored Core SHA-256:
  `0B34622FE890F2BDF665F26D1CEF5120D139A6D365F66CAEF6A11993C2F529F2`
- Core programming and verify: PASS.
- OpenOCD was shut down; no OpenOCD process remained.
- No commit, push, tag, remote change, IOC regeneration, generated-source
  edit, or Vault write was performed.

## F407 TIM1/PE11 servo vertical slice

Status: **PASS in the authorized attended scope**.

### Architecture changes

- Replaced the HAL-coupled shared PWM API with a board-neutral contract:
  opaque channel identity plus `start`, `stop`, `set_period_us`,
  `set_pulse_us`, and `set_duty`.
- The F407 board backend alone owns TIM1, channel 2, PE11 AF1, clock setup,
  HAL calls, and teardown.
- Added a dedicated `PNX_ENABLE_PWM_A2` application closure. Core, USB, and
  CAN Ninja graphs contain zero `pwm_backend.cpp` references.
- The board IOC and CubeMX-generated sources were not modified or regenerated.
- The test application has no motor layer, service, factory, registry, or
  persistent control interface.

### Software validation

- Host tests: 20/20 PASS, including the PWM contract and exact bounded servo
  sequence.
- Embedded Debug builds PASS with zero observed compiler/linker warnings:
  Core, USB CDC, CAN/M2006, and PWM/A2.
- PWM/A2 ELF SHA-256:
  `C3BF015E8354CD31369A031419CB676CFA33D92AAF425E32B61FE893D0E5EC5A`.

### Attended hardware validation

- OpenOCD program/verify/reset: PASS.
- Commanded sequence:
  `1500 -> 1450 -> 1500 -> 1550 -> 1500 us`, followed by a final center hold
  and output disable.
- Operator observation: the connected servo moved.
- Final telemetry:
  heartbeat `5`, step count `5`, pulse `0`, complete `1`, faulted `0`,
  output enabled `0`.
- Final register snapshot showed the TIM1 peripheral clock disabled after
  completion.

### Safe restoration

- The board was restored to the normal Core Debug image.
- Restored Core ELF SHA-256:
  `6D0CEFDCD1CE31DD4E0074813B4596912B2AFE6B4D6E278B0AE97EC83352964C`.
- Core programming and verify: PASS.
- No OpenOCD process remained.
- No commit, push, tag, remote change, IOC/generated edit, or Vault write was
  performed.

### Known integration boundary

- `pnx_bsp` is still at detached HEAD
  `f61e8ac8ac4b75d93b021ec32a8ecf56fc36a73b`; the PWM contract change is an
  uncommitted submodule working-tree modification.
- Dormant BMI088 code still names the former timer-specific PWM channels.
  Its board binding must be migrated when the BMI088 vertical slice begins;
  it was intentionally not pulled into this servo-only product graph.

## F407 shared-peripheral completion and provenance

Status: **software complete; BMI088 hardware PASS; DBUS hardware deferred by
operator direction**.

This section supersedes the earlier notes that the shared submodules were
dirty detached worktrees and that the BMI088 device driver still contained
timer-specific or H7-specific bindings.

### Architecture result

- `pnx_bsp` SPI, PWM, and Flash contracts are board-neutral. STM32 HAL types
  and calls exist only in C-board backends.
- The F407 SPI backend owns SPI1 Mode 3 and the PA7/PB3/PB4 pins. Board
  composition owns PA4 accelerometer CS and PB0 gyro CS.
- BMI088 register transport is independent of STM32 and has explicit injected
  bus and chip selects. The higher-level BMI088 driver also injects its
  optional heater PWM channel and data-ready attachment hook.
- The dormant WS2812 driver no longer hardcodes H7 `spi6`.
- F407 Flash sector geometry and HAL erase/program logic are board-local.
  No destructive Flash hardware test was run because this repository has no
  reserved test sector.
- H7 memory-bank labels `RAM_D1/D2/D3` were removed from shared code. Actual
  DMA buffers use `PNX_DMA_BUFFER`; ordinary objects use normal BSS; retained
  diagnostics use `.noinit`.
- The F407 USART backend uses generated USART handles only below the board
  boundary. The DR16 decoder parses the 18-byte wire format explicitly rather
  than relying on packed compiler bitfields.
- The automatic M2006 pulse image is no longer a normal preset. It is
  available only through the explicit
  `PNX_ENABLE_CAN_M2006_VALIDATION=ON` attended-validation switch.
- The default motor CAN bus is the board-neutral `bsp::can::bus::none`; no
  `fdcan1` reference remains in shared source.

### Formal submodule provenance

The following local branches and commits were created. They have **not** been
pushed:

- `pnx_bsp` `refactor/f407-portable-peripherals`:
  `899c0b491ec81e48ba8ed66ae90d451fb2bddc1b`
- `pnx_devices` `refactor/f407-portable-devices`:
  `e90601d8676922db954c90a06de97ef82d87bf01`
- `pnx_libs` `refactor/f407-memory-semantics`:
  `205217a42ac4e1e556e63f594eb7671f47e0e2b9`
- `pnx_modules` `refactor/f407-remoter-protocol`:
  `deaae6720a6051c0276b1ffbeb3fc0fc875456dd`

The parent integration records these four SHAs as gitlinks. The pre-existing
untracked `testing.md` remains outside Git.

### Final software validation

- Clean embedded builds: 7/7 PASS:
  `f407-debug`, `f407-release`, `f407-usb-cdc-debug`,
  `f407-usb-cdc-release`, `f407-pwm-a2-debug`,
  `f407-bmi088-debug`, and `f407-dbus-rx-debug`.
- Explicit attended CAN validation build: PASS using
  `PNX_ENABLE_CAN_M2006_VALIDATION=ON`.
- Host tests: 25/25 PASS, including USB, CAN, M2006 sequence, PWM, SPI,
  BMI088 transport, Flash contract, USART contract, and DR16 protocol.
- Compiler/linker warnings observed in the clean builds: zero.
- Core Ninja graph optional closure: none.
- Shared-source leakage scan found zero STM32 HAL types, `stm32h7`, `fdcan1`,
  old timer/SPI enum bindings, or `RAM_D1/D2/D3` names in
  `pnx_bsp`, `pnx_devices`, `pnx_libs`, and `pnx_modules`.
- `git diff --check`: PASS; Git only reported the repository's existing
  LF-to-CRLF checkout policy notices.

Final artifact hashes:

- Core Debug:
  `4736B2E1B0EFC47CD2A3D94CB10112EBA362DC57FADF38188BAB3B5A8C427BE1`
- BMI088 Debug:
  `F7E32A275CDE72D309700A97285DDAB1557A777BE3E524A35F0AFA4FF2535C02`
- DBUS RX Debug:
  `2E86C49C6FBF7AC54FDCD88F66611AF125CB6DCD9AE4D7F692854F055E01FF6A`
- Explicit CAN validation:
  `24B7165407F9EF2E8727CFE4AE1EC192CC3F8BDC59F6C586856A6ABAE596A458`

### Final BMI088 hardware evidence

- Final BMI088 artifact program/verify/reset: PASS.
- Runtime symbol:
  `demo::cboard::imu_bmi088::runtime` at `0x2000381C`.
- Snapshot A:
  heartbeat/sample/change `0x7DB/0x7DB/0x7DA`, IDs `0x1E/0x0F`,
  complete/faulted `1/0`.
- Snapshot B one second later:
  heartbeat/sample/change `0x865/0x865/0x864`, IDs `0x1E/0x0F`,
  complete/faulted `1/0`.
- All six raw axes changed between the two snapshots.

Result:
`SPI1 -> BMI088 device ID -> accelerometer/gyroscope raw data` is **PASS on
hardware** for the final source state.

### Final safe restoration and deferred evidence

- The board was restored to the final Core Debug artifact.
- Core program/verify/reset: PASS.
- Final Core sample: PC in ThreadX PendSV, DWT control/counter
  `0x40000001/0x1816AC86`, GPIOH ODR `0x800`.
- OpenOCD process count after resume/shutdown: zero.
- `DBUS_LIVE_FRAME`: **NOT RUN by explicit operator direction**.
- UART3/DBUS hardware behavior must not be presented as verified; only its
  backend build, USART contract, parser, and remote-module integration are
  verified.
- No remote push, tag, IOC edit, generated-code edit, destructive Flash test,
  CAN output, or PWM output was performed during this completion pass.

## 2026-07-29 architecture release-candidate cleanup

### Fact: scope and intent

This change is a narrow release-hygiene cleanup on top of the recorded F407
vertical-slice evidence. It does not alter IOC/generated files, startup,
linker, HAL runtime code, SPI1/BMI088 source, TIM1/PWM source, USB runtime
code, or application behaviour.

- Removed the unused H7-template SPI2/SPI6/TIM3/TIM12 capability parsing and
  generated SPI/PWM configuration from `configs/cmake/import_ioc.cmake` and
  `configs/cmake/generate_config.cmake`.
- Removed the unselected Flash public/backend closure and F4 Flash HAL source
  files from the default Core source graph. The board-local Flash backend is
  retained for a future explicit Flash slice; it remains unverified on
  hardware because no reserved destructive-test sector exists.
- Made every normal CMake preset explicitly set all optional application
  selectors. An attended CAN configure can therefore no longer silently
  persist into a subsequently selected normal Core/USB/PWM/BMI/DBUS preset.
- Updated the root README, repository rules, and board ownership note. The
  documents distinguish product images from attended validation closures and
  give one authoritative location for manual SPI1/TIM1 ownership.

### Fact: fresh software validation after the cleanup

On 2026-07-29, from a fresh CMake configure directory for each normal preset:

- 7/7 embedded builds PASS: `f407-debug`, `f407-release`,
  `f407-usb-cdc-debug`, `f407-usb-cdc-release`, `f407-pwm-a2-debug`,
  `f407-bmi088-debug`, and `f407-dbus-rx-debug`.
- The explicit attended CAN/M2006 image PASSed in its own binary directory
  `build/f407-can-m2006-debug` with all other optional closures OFF.
- Host tests: 25/25 PASS.
- `ninja -C build/f407-debug -t commands` contains none of
  `flash_backend`, `bsp_flash`, `usb_backend`, `can_backend`, `spi_backend`,
  `pwm_backend`, `bmi088`, or `usart_backend`.
- The removed generated-configuration identifiers have zero matches under
  `configs`; `git diff --check` PASSed.

### Inference: release status

The source tree is suitable for an **F407 architecture release candidate**:
the normal Core graph is now materially minimal, configuration authority is
clear, and optional hardware slices do not leak into it. The prior hardware
evidence remains relevant because this cleanup did not change the exercised
runtime code, but no new binary was programmed in this cleanup pass.

### Remaining release gates

- `DBUS_LIVE_FRAME=NOT_RUN`; DBUS remains software-validated only.
- Fresh-clone/submodule retrieval must be verified after the recorded
  submodule commits and parent commit are available from `origin`.
- A public release still requires a repository-owner decision on LICENSE,
  NOTICE, and third-party distribution terms. No legal metadata is invented
  by this engineering cleanup.

### Fact: submodule clone URL correction

The initial remote fresh-clone attempt reached the parent and public
`pnx_bsp`, but failed before checkout because `pnx_devices`, `pnx_libs`, and
`pnx_modules` were recorded with unavailable `git@github.com:HKUSTGZ-ROBOMASTER-PNX/...`
SSH URLs. Their tested release remotes are the corresponding
`https://github.com/shiuhou/...` repositories, which already contain the
recorded F407 branches. `.gitmodules` is therefore corrected to these HTTPS
URLs before the final fresh-clone retry. This is release provenance repair,
not an architecture or firmware behaviour change.

### Fact: final remote reproducibility evidence

After the URL correction, a clean clone of
`refactor/f407-minimal-architecture@4a3a101a3d201b3d61cad71df730ed059559b7e0`
with `--recurse-submodules` completed from `origin`. It checked out exactly:

- `pnx_bsp@899c0b491ec81e48ba8ed66ae90d451fb2bddc1b`
- `pnx_devices@e90601d8676922db954c90a06de97ef82d87bf01`
- `pnx_libs@205217a42ac4e1e556e63f594eb7671f47e0e2b9`
- `pnx_modules@deaae6720a6051c0276b1ffbeb3fc0fc875456dd`

In that independent checkout, all seven normal embedded presets and the
isolated CAN attended image built successfully; `ctest` reported 25/25 host
tests PASS. The only observed transient was one HTTPS connection timeout while
cloning `pnx_modules`; Git retried automatically and checkout completed.
