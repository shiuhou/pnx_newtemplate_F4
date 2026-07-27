# Pure-F407 Validation

This is the primary validation ledger. It separates retained software
evidence, individual attended hardware labs and the exact current combined
tree. The software-only machine-readable record in
[release/pnx-f4-local-validation.json](../release/pnx-f4-local-validation.json)
predates the current combined tree and is retained evidence, not a fresh
checkpoint result.

## Result ledger

```text
SIX_F407_PRESET_BUILDS=RETAINED_PASS
CURRENT_COMBINED_TREE_PRESET_REGRESSION=NOT_RUN
HOST_TEST_BASELINE=RETAINED_34_OF_34_PASS
EXPANDED_CURRENT_HOST_SUITE=NOT_RUN
PURE_F407_CUBEMX_GATE=RETAINED_PASS
PURE_F407_BOUNDARY=RETAINED_PASS
PURE_F407_NO_H723=RETAINED_PASS
PURE_F407_ELF_MAP_GATE=RETAINED_PASS
PURE_F407_DESCRIPTOR_FAIL_CLOSED=RETAINED_PASS
PURE_F407_USB_CONTROLLER_STOPPED=RETAINED_PASS
PURE_F407_MOTOR_DEFAULT_ZERO=RETAINED_PASS
PURE_F407_HW1=PASS
PURE_F407_PROGRAM_VERIFY=PASS
RESET_HANDLER_PATH=PASS
MAIN_REACHED=PASS
THREADX_INIT_REACHED=PASS
APP_START_REACHED=PASS
HARDFAULT_NOT_ENTERED=PASS
LED_RUNTIME=PASS
THREADX_RUNTIME=PASS
DWT_RUNTIME=PASS
STACK_BASELINE=PASS
FAULT_BASELINE=PASS
DEBUG_RESET_RECOVERY=PASS
BOARD_RESET_RECOVERY=NOT_RUN
UART_TELEMETRY=SKIPPED_BY_USER_NOT_BLOCKING
PURE_F407_USB_PREFLIGHT=PASS
USB_LAB_IDENTITY=APPROVED_ISOLATED_LAB_ONLY
USB_CDC=ISOLATED_LAB_PASS
USB_DESCRIPTOR_IDENTITY=UNASSIGNED_FAIL_CLOSED
F407_CAN1_RECEIVE=PASS
F407_M2006_BOUNDED_ACTUATION=PASS
F407_PWM_C2_SERVO=BOUNDED_PHYSICAL_OBSERVATION_PASS
F407_BMI088_REPLACEMENT_BOARD_LAB=LAB_PASS
F407_IST8310_LAB=LAB_PASS
F407_COMMON_SENSOR_RUNTIME=NOT_DONE
F407_CAN2=NOT_RUN_OPTIONAL
F407_DBUS=NOT_IMPLEMENTED_NOT_RUN
F407_MAIXCAM=NOT_RUN
F407_AHRS=OUT_OF_SCOPE
DAILY_INTEGRATED_FIRMWARE=PARTIAL
F407_ONLY_TEAM_RELEASE=NOT_PUBLISHED
```

No build or test was rerun while preparing this checkpoint.

## Retained software baseline

The following results are from the 2026-07-27 accepted run. They are not
results of the exact current combined tree.

### Firmware artifacts

Memory values are the linker `--print-memory-usage` results. That retained run
contained zero compiler or linker warnings.

| Preset | Output directory | ELF SHA-256 | Flash | RAM | CCMRAM |
|---|---|---|---:|---:|---:|
| `board-smoke` | `build/dji-c-board` | `1C8C9A0CF5C74DCD37A02346E43A942933701EA495AE7CB6C4106DA07A7CE192` | 44504 B | 52968 B | 0 B |
| `can-rx` | `build/dji-c-board-can-rx` | `043CF9251A2FD27962F6EF4CD135E6DEF21A926F11A87638DEA6DB23293C3F97` | 70432 B | 60832 B | 0 B |
| `motor-safe` | `build/dji-c-board-motor-safe` | `F5CD83A612389895341DB49BF9F048F2E9097695C5262F0EFC8C12C1C0C95604` | 71080 B | 60856 B | 0 B |
| `board-smoke-release` | `build/dji-c-board-release` | `D933CE7A22AA619C54E55200AA2C1FA4589291C642D7F02F5F86E52F9EF90573` | 25920 B | 52968 B | 0 B |
| `usb-cdc` | `build/dji-c-board-usb-cdc` | `C6B4F863F60EEDA918711D41142CCB1C59012E6671979EB0CD0F793601141FF7` | 85200 B | 71072 B | 0 B |
| `usb-cdc-release` | `build/dji-c-board-usb-cdc-release` | `DF21F2073317355066624C0DA5B56C727EE0BCB8E25C98D5264CEDE62859D190` | 50076 B | 70976 B | 0 B |

Every build directory contains ELF, HEX, BIN, MAP, and the actual-target
source inventory. Every ELF is little-endian ARM ELF32 and contains
`Reset_Handler`, `main`, `tx_application_define`, `app_start`, and
`HardFault_Handler`. No H723 or legacy `board/` source entered any build graph.

## Isolated-lab USB image

This local image is outside the six tracked firmware presets:

| Field | Value |
|---|---|
| ELF | `build/dji-c-board-usb-cdc-lab/pnx_embedded.elf` |
| SHA-256 | `0AF3F14695F3CD15EB06690CB719C161A1FB7A431E3E40F2F086D94B59676DC2` |
| VID / PID | `0xCAFE` / `0xF407` |
| Flash / RAM / CCMRAM | 91,996 B / 71,088 B / 0 B |
| Scope | one-board isolated-lab testing only; not assigned or publishable |

The identity input is retained in ignored
`build/usb-lab/identity.cmake`. The tracked default and release profiles remain
fail-closed. A fresh default `usb-cdc` rebuild reproduced
`C6B4F863F60EEDA918711D41142CCB1C59012E6671979EB0CD0F793601141FF7`,
with identity confirmation off, zero VID/PID and serial `UNASSIGNED`.

### Executed checks

These checks belong to the retained software baseline:

| Check | Result |
|---|---|
| two immutable export candidates | both candidate gates `PASS`; Git tree and canonical SHA-256 identical |
| promoted workspace gate | `PASS` |
| CubeMX production gate | `PASS`; six F407 presets and four clean exact submodules |
| board boundary gate | `PASS` |
| F407 USB source acceptance | `PASS` (9/9) |
| host CTest | `PASS` (34/34) |
| descriptor identity | unconfirmed, VID/PID `0x0000`, serial `UNASSIGNED`; `PASS` |
| fail-closed USB link | `bsp::usb::init` absent from both USB images; controller remains stopped |
| motor-safe configuration | non-zero test disabled; bus/model `none`, ID/current `0`; guard test `PASS` |
| JSON, Markdown, licence, credential, temporary-file and tracked absolute-path checks | `PASS` |

## Board Smoke hardware validation

The tested artifact was
`build/dji-c-board/pnx_embedded.elf`, SHA-256
`1C8C9A0CF5C74DCD37A02346E43A942933701EA495AE7CB6C4106DA07A7CE192`,
from branch `main` at
`87bc832c7f368b1243459de5e3afe6c7fcc0b7da`.

| Check | Observed result |
|---|---|
| CMSIS-DAP | CMSIS-DAPv2, serial `[REDACTED_LOCAL_DEBUGGER_SERIAL]`, firmware `Horco v0.1` |
| SWD | 1000 kHz, DPIDR `0x2ba01477`, Cortex-M4 r0p1 |
| Target | STM32 device ID `0x413`, 1024 KiB Flash |
| Program / verify / reset | `Programming Finished`; `Verified OK`; target reset |
| Startup chain | `Reset_Handler` -> `main` -> `tx_application_define` -> `app_start` |
| Fault registers | CFSR `0`, HFSR `0`; precise HardFault breakpoint not entered |
| Physical LED | operator observed regular green heartbeat |
| Runtime samples | heartbeat `82 -> 88`; tick `40500 -> 43500`; DWT advanced by approximately 3 seconds |
| Thread / stack | thread run count `82 -> 88`; stack free `1448 B`; byte pool available `11256 B` |
| Fault baseline | application fault `0`; valid crash record `0`; UART error `0` |
| Debug reset recovery | after 3 seconds: heartbeat `7`, tick `3000`, stack free `1480 B`, fault `0` |
| Physical reset button | `NOT_RUN` |
| UART receive capture | `SKIPPED_BY_USER_NOT_BLOCKING`; MCU-side UART init succeeded and transmit count increased |
| USB enumeration / CDC | `NOT_RUN` at the Board Smoke stage; later isolated-lab results are recorded below |

## Retained USB preflight state

This subsection records the state immediately before the isolated USB lab; its
`NOT_RUN` values are historical and are superseded by the lab result below.

- Power: C-board micro-USB is the sole board-power source.
- Disconnected: 24 V, CAN, motors, PWM and DBUS.
- SWD: debug signals, ground and voltage sensing only; no second target-power
  source.
- Lab identity: approved for isolated local testing only.
- Enumeration, descriptor capture, CDC transfer and reconnect: `NOT_RUN` at
  preflight time.
- Static/software checks: promoted workspace `PASS`, CubeMX production
  `PASS`, USB source acceptance `PASS` (9/9), host CTest `PASS` (34/34).

Retained ignored evidence:

```text
build/hardware-evidence/pure-f407-hw1-hw2/hw1-program-verify-reset.log
build/hardware-evidence/pure-f407-hw1-hw2/hw2-openocd-server.stderr.log
build/hardware-evidence/pure-f407-hw1-hw2/hw2-startup-chain-sequential.log
build/hardware-evidence/pure-f407-hw1-hw2/hw2-runtime-sample-1.log
build/hardware-evidence/pure-f407-hw1-hw2/hw2-runtime-sample-2.log
build/hardware-evidence/pure-f407-hw1-hw2/hw2-debug-reset.log
build/hardware-evidence/pure-f407-hw1-hw2/hw2-debug-reset-recovery.log
```

The earlier two multi-breakpoint GDB attempts are retained as negative
evidence. They stopped after `main` because duplicated symbol locations
exhausted the hardware breakpoint comparators; sequential temporary
breakpoints resolved the debugger limitation without changing firmware.

## USB, CAN1 and M2006 attended hardware validation

The isolated-lab USB device enumerated as `VID_CAFE&PID_F407` on Windows.
CDC banner/echo, bidirectional transfer, close/reopen and reset
re-enumeration passed. This does not assign or authorize a production USB
identity.

After the CAN FIFO0 callback was changed to drain all pending frames, the live
DREDGE source produced 9,651 received frames with CAN error/drop `0/0`.
The zero-output motor-safe run produced 12,394 feedback frames, 6,100 zero
transmissions, zero non-zero transmissions and no error/drop/fault.

The original M2006 task requested `+100` as its historical initial scope. A
later separately approved extension used CAN1 feedback ID `0x203` and sent a
`+500` raw command for no more than 250 ms through a one-shot USB arm gate.
Telemetry recorded 125 non-zero transmissions, automatic return to zero,
latched completion and CAN error/drop/fault `0/0/0`. The operator observed one
very short movement and no sustained motion. Full evidence is in
`.codex/tasks/2026-07-28-m2006-bounded-pulse/validation-report.md`.

The board was then restored to `board_smoke`, with non-zero motor testing OFF
and USB identity confirmation OFF. The restored ELF SHA-256 is
`53C373C65C332011C561340DA590068AEA47BAFA201E5787BCE41FEDEF9C4538`.

Current workspace commands:

```powershell
python -B scripts/check_f407_only.py --repo-root . --promoted-workspace
python -B scripts/check_cubemx_production.py --repo-root . --check-submodules-clean
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build_dji_c_board.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_host.ps1
```

The default candidate-mode checker applies only to the two immutable
deterministic export directories. It intentionally does not accept the
promoted documentation/configuration overlay. The immutable
`release/f407-only-provenance.json` remains an F4-2 candidate snapshot with its
original `NOT_RUN` values; current results are recorded in this document and
the local validation manifest.

## PWM C2 attended hardware observation

The operator observed four small servo movements from the demo-local sequence.
This is `F407_PWM_C2_SERVO=BOUNDED_PHYSICAL_OBSERVATION_PASS` only. It does not
establish quantitative angle, pulse-width or load accuracy. The expanded host
suite and repository-wide combined regression remain `NOT_RUN`.

## BMI088 attended hardware validation

The isolated `imu_bmi088_lab` built and its helper host test passed. It used
the manual/reference SPI1 boundary without modifying the IOC or public BSP.
With USB-only power, and again with 24 V connected plus a measured 5 V
B2-A2 rail, both chip IDs remained `0xFF`. A bounded Mode 0/1/2/3 scan also
returned only `0xFF`; continuous raw axes were all `-1` while ThreadX and SPI
transactions continued.

Result:
`STEER-PURE-F407-IMU-BMI088=BLOCKED_BY_BMI088_HARDWARE_NO_RESPONSE`.
No valid sensor data was observed. The board was restored to Board Smoke,
SHA-256
`781CD46356C55B9B397AB7AB8751EAF7B45CB1A6F92588681834C3C26930CDA4`.

### Replacement-board retest

The first-board result above is retained as negative hardware evidence. The
replacement board returned accelerometer/gyro IDs `0x1E/0x0F`, runtime state
`2`, fault `0`. Its stationary raw accelerometer sample was
`(74, -154, 5417)` and changed to `(-5133, 1532, 724)` after an
operator-confirmed 90-degree tilt; gyro changed from `(15, 12, 38)` to
`(43, -102, -250)`. Sample/change counts reached `42641/31896`.

Final result:
`F407_BMI088_REPLACEMENT_BOARD_LAB=LAB_PASS`. This is individual lab
evidence, not formal common-runtime integration. The safe Board Smoke image
was restored after the measurement.

## IST8310 attended hardware validation

The replacement board returned IST8310 device ID `0x10` over the C-board
I2C3 pins, with data-ready `1` and fault/I2C error `0/0`. The stationary raw
sample `(2, -159, 168)` changed to `(-31, -174, -49)` after an
operator-confirmed horizontal 90-degree rotation. The second capture followed
10,017 successful samples.

Result: `F407_IST8310_LAB=LAB_PASS`. This is individual lab evidence, not
formal common-runtime integration. Board Smoke was restored after the
measurement.

## Formal integration boundary

```text
F407_COMMON_SENSOR_RUNTIME=NOT_DONE
F407_UART_CAPTURE=SKIPPED_BY_USER_NOT_BLOCKING
F407_CAN2=NOT_RUN_OPTIONAL
F407_DBUS=NOT_IMPLEMENTED_NOT_RUN
F407_MAIXCAM=NOT_RUN
F407_AHRS=OUT_OF_SCOPE
DAILY_INTEGRATED_FIRMWARE=PARTIAL
F407_ONLY_TEAM_RELEASE=NOT_PUBLISHED
```

The individual PWM, BMI088 and IST8310 lab sources are not formal production
integration.

## 2026-07-28 pre-integration software regression

Tested code commit:
`730b987c12f8951e8b0e2a0d1b9e655d0a585dff`.

- All six official F407 presets rebuilt successfully with zero compiler or
  linker warnings.
- The complete current host suite passed 39/39, with zero failed and zero
  skipped tests.
- The promoted F407-only, CubeMX production and board-boundary gates passed.
- `pwm_servo_lab`, `imu_bmi088_lab` and `ist8310_mag_lab` each configured and
  built successfully through the existing `board-smoke` CMake interface; each
  produced its ELF with zero observed warnings.
- Tracked content remained clean except for the approved
  `CMakePresets.json` stat-only status; its textual diff was empty. All four
  submodules remained at their recorded gitlinks.
- No hardware was operated. Existing hardware results remain retained
  historical or isolated-lab evidence.

Formal BMI088 + IST8310 integration remains `NOT_DONE`; UART remains
`SKIPPED_BY_USER_NOT_BLOCKING`.
