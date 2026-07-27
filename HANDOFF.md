# Engineering Handoff: Local Pure-F407 Workspace

Updated: 2026-07-27

## Repository state

- Role: active local pure-F407 DJI C-board workspace
- Branch: `checkpoint/pure-f407-pre-sensor-integration`
- Checkpoint starting HEAD:
  `87bc832c7f368b1243459de5e3afe6c7fcc0b7da`
- Materialized candidate commit:
  `924697c7312e0c8425f563bce13fe517cb0847b8`
- Superproject remote: none
- Cross-machine clone: deferred

## Source provenance

- Accepted firmware source:
  `bc31a22616c598d81b19d95c7c7d5877cb1a5f94`
- Accepted source-manifest commit:
  `48c6370b8d3e043f7f853a1c89ca2b4a3b79a525`
- Exporter commit:
  `8b1d1683b753d01e418d337df3155bf4a11801ce`
- Candidate canonical SHA-256:
  `7003205C6D77E65E796BFD4A7A1DFE10F5D5B0C3BAF2C6514AC15AB9446BE16D`

The local submodules were initialized through untracked overrides in this
repository's Git metadata. Tracked `.gitmodules` retains HTTPS URLs.

## Acceptance boundary

```text
PURE_F407_LOCAL_REPOSITORY=PASS
LOCAL_SUBMODULE_INITIALIZATION=PASS
PURE_F407_SOURCE_PROVENANCE=PASS
PURE_F407_LOCAL_SOFTWARE_ACCEPTED=PASS
MULTIBOARD_F407_HW1=PASS_HISTORICAL
PURE_F407_HW1=PASS
PURE_F407_PROGRAM_VERIFY=PASS
F407_CORE_RUNTIME=PASS
LED_RUNTIME=PASS
THREADX_RUNTIME=PASS
DWT_RUNTIME=PASS
FAULT_BASELINE=PASS
SIX_F407_PRESET_BUILDS=RETAINED_PASS
CURRENT_COMBINED_TREE_REGRESSION=NOT_RUN
HOST_TEST_BASELINE=RETAINED_34_OF_34_PASS
EXPANDED_CURRENT_HOST_SUITE=NOT_RUN
UART_TELEMETRY=SKIPPED_BY_USER_NOT_BLOCKING
PURE_F407_USB_PREFLIGHT=PASS
USB_DEVELOPMENT_IDENTITY=APPROVED_ISOLATED_LAB_ONLY
USB_CDC=ISOLATED_LAB_PASS
CAN1_RECEIVE=PASS
M2006_BOUNDED_PULSE=PASS
PWM_C2_SERVO=BOUNDED_PHYSICAL_OBSERVATION_PASS
BMI088_REPLACEMENT_BOARD_LAB=LAB_PASS
IST8310_LAB=LAB_PASS
FORMAL_COMMON_SENSOR_RUNTIME=NOT_DONE
CAN2=NOT_RUN_OPTIONAL
DBUS=NOT_IMPLEMENTED_NOT_RUN
MAIXCAM=NOT_RUN
AHRS=OUT_OF_SCOPE
DAILY_INTEGRATED_FIRMWARE=PARTIAL
F4_2_FRESH_CLONE=BLOCKED_BY_SUBMODULE_REACHABILITY_HISTORICAL
CROSS_MACHINE_CLONE=DEFERRED
F407_ONLY_TEAM_RELEASE=NOT_PUBLISHED
```

The pure-F407 Board Smoke ELF was programmed on 2026-07-27 using CMSIS-DAPv2
serial `[REDACTED_LOCAL_DEBUGGER_SERIAL]`. OpenOCD reported SWD DPIDR
`0x2ba01477`, Cortex-M4
r0p1, STM32 device ID `0x413`, 1024 KiB Flash, `Programming Finished`,
`Verified OK`, and target reset. GDB then reached `Reset_Handler`, `main`,
`tx_application_define`, and `app_start` without entering the precise
HardFault breakpoint.

The operator observed the green Board Smoke heartbeat. Two runtime samples
showed heartbeat `82 -> 88`, ThreadX tick `40500 -> 43500`, monotonically
increasing DWT time, thread run count `82 -> 88`, 1448 bytes of stack free,
zero application faults, zero valid crash record, and zero CFSR/HFSR. A
debugger reset returned to `Reset_Handler`; after three seconds ThreadX was
running again with heartbeat `7`, tick `3000`, stack free `1480`, and no
fault. Physical UART capture, board-button reset, and all USB hardware tests
remain unrun.

USB preflight was approved on 2026-07-27. The first USB run uses the C-board
micro-USB connector as the sole board-power source; 24 V, CAN, motors, PWM and
DBUS remain disconnected. SWD may remain attached only for debug signals,
ground and target-voltage sensing, without a second target-power source.

The approved development identity is isolated-lab-only:
`VID=0xCAFE`, `PID=0xF407`. It is not an assigned or publishable identity.
The remaining identity input and firmware artifacts are retained only under
ignored `build/` paths. The tracked default and release USB presets remain
`VID/PID=0x0000`, serial `UNASSIGNED`, controller stopped.

## Actual changes after deterministic materialization

- established the concise pure-F407 documentation chain;
- made build output directories stable for Board Smoke and USB CDC;
- added repository-relative VS Code Board Smoke and USB debug configurations;
- added a promoted-workspace static-check mode and four regression tests;
- registered the promoted Gate and regression tests in host CTest;
- retained ignored local build/validation logs under `build/logs/`;
- retained fail-closed descriptor and zero-motor defaults;
- added a VS Code debug entry for the ignored isolated-lab USB image;
- built the isolated-lab image without placing its identity in a tracked
  preset or release manifest.

No firmware implementation, IOC, generated closure, public API, shared
submodule, or gitlink changed.

## Validation

Retained software results, artifact hashes and individual hardware labs are
recorded in [docs/VALIDATION.md](docs/VALIDATION.md). Pure-F407 HW-1 and the
core runtime baseline pass. USB CDC passed only with the isolated-lab
identity; no production USB identity or release approval exists.

In the retained 2026-07-27 baseline, all six F407 firmware presets built with
zero warnings and host CTest passed 34/34. The CubeMX production,
board-boundary, no-H723, ELF/MAP, descriptor-fail-closed,
controller-stopped, motor-zero, licence, JSON, Markdown, and
tracked-cleanliness gates also passed in that run. The exact current combined
tree has not rerun the six presets or expanded host suite. The retained Board
Smoke ELF is:

```text
build/dji-c-board/pnx_embedded.elf
SHA-256=1C8C9A0CF5C74DCD37A02346E43A942933701EA495AE7CB6C4106DA07A7CE192
```

The isolated-lab USB ELF is:

```text
build/dji-c-board-usb-cdc-lab/pnx_embedded.elf
SHA-256=0AF3F14695F3CD15EB06690CB719C161A1FB7A431E3E40F2F086D94B59676DC2
```

Its clean build used 91,996 bytes of Flash, 71,088 bytes of RAM and zero
CCMRAM. The compile database records identity confirmation enabled and the
approved lab values; the ELF contains `bsp::usb::init`, `HAL_PCD_Start`, the
USBX device stack and the three lab strings. A fresh rebuild of the tracked
default `usb-cdc` preset retained its accepted fail-closed SHA-256
`C6B4F863F60EEDA918711D41142CCB1C59012E6671979EB0CD0F793601141FF7`.

`release/f407-only-provenance.json` is the immutable F4-2 export snapshot. Its
candidate-time `NOT_RUN` values are retained as provenance and are not the
ledger for this promoted workspace; current results are recorded here and in
`docs/VALIDATION.md`.

## Next action

After this checkpoint is accepted, run the exact current combined-tree
software regression. Formal BMI088 + IST8310 common runtime is a separate
integration step. Do not treat the individual lab sources as production
sensor integration.

## Rollback

The deterministic materialization/base commit is
`924697c7312e0c8425f563bce13fe517cb0847b8`; the current HEAD is the local
commit containing this handoff. No remote, push, tag, source, IOC, generated
closure, submodule, or Vault action occurred. Hardware was used only for the
bounded Board Smoke SWD/core-runtime validation described above.

## 2026-07-28 attended hardware update

Isolated-lab USB enumeration, CDC echo/reopen/reset re-enumeration, CAN1
high-rate receive, motor zero-output and one bounded M2006 actuation passed.
The accepted pulse was `+500` raw for at most 250 ms: 125 non-zero group
frames, automatic return to zero, one-shot latch, CAN error/drop/fault
`0/0/0`, and one operator-observed very short movement without sustained
motion. The original task's `+100` request remains the historical initial
scope; the approved and executed `+500` / 250 ms pulse was a later bounded
extension. CAN2 was left `NOT_RUN / OPTIONAL`; UART capture was skipped by
user choice and is not blocking.

The board was restored to `board_smoke` with non-zero motor testing OFF and
USB identity confirmation OFF. Restored ELF SHA-256:
`53C373C65C332011C561340DA590068AEA47BAFA201E5787BCE41FEDEF9C4538`.
No commit, push, remote, tag, submodule or Vault action was performed during
this attended sequence.

## 2026-07-28 BMI088 attended hardware update

`STEER-PURE-F407-IMU-BMI088` is
`BLOCKED_BY_BMI088_HARDWARE_NO_RESPONSE`, not PASS. A demo-local SPI1 lab
matched the C-board manual and retained reference: PA4/PB0 chip selects,
PB3/PB4/PA7 SPI1, Mode 3 and 10.5 Mbit/s. ThreadX and SPI transactions ran,
but both dies returned `0xFF`; all six raw axes were `-1`.

The result remained unchanged with 24 V connected and an operator-measured
5 V between PWM B2 and A2. A bounded scan of SPI modes 0 through 3 returned
`0xFF` for all eight ID reads. Remaining physical suspects are local IMU
3.3 V delivery, soldering/connectivity or the BMI088 itself. Full evidence is
in `.codex/tasks/2026-07-28-bmi088-imu/validation-report.md`.

The board was restored to the safe `board_smoke` image, SHA-256
`781CD46356C55B9B397AB7AB8751EAF7B45CB1A6F92588681834C3C26930CDA4`.

## 2026-07-28 IST8310 attended hardware update

`STEER-PURE-F407-IST8310-MAG=PASS` on the replacement C-board. A demo-local
400 kHz I2C3 closure used PA8 SCL, PC9 SDA and PG6 reset without changing the
IOC or public BSP. Device ID was `0x10`, data-ready was `1`, and fault/I2C
error remained `0/0`.

The stationary raw sample `(2, -159, 168)` changed to
`(-31, -174, -49)` after an operator-confirmed horizontal 90-degree rotation.
The second capture followed 10,017 successful samples. Full evidence is in
`.codex/tasks/2026-07-28-ist8310-mag/validation-report.md`.

Board Smoke was restored afterward with SHA-256
`781CD46356C55B9B397AB7AB8751EAF7B45CB1A6F92588681834C3C26930CDA4`.
No public BSP, IOC, generated closure, submodule, remote or Vault was changed;
no commit, tag or push was made.

### BMI088 replacement-board retest

The first-board failure above remains valid negative evidence. On a
replacement C-board, the same lab returned the required accelerometer/gyro
IDs `0x1E/0x0F`, state `RUNNING`, fault `0`. After preserving the
post-soft-reset SPI selection and power delays, the stationary raw sample was
accelerometer `(74, -154, 5417)` and gyro `(15, 12, 38)`.

After the operator tilted the board about 90 degrees, the sample became
accelerometer `(-5133, 1532, 724)` and gyro `(43, -102, -250)`. The gravity
axis therefore moved from approximately `+Z` to `-X`; sample/change counts
reached `42641/31896`.

`STEER-PURE-F407-IMU-BMI088=PASS` on the replacement board. This proves the
onboard BMI088 and demo-local SPI1 runtime boundary; formal integration of
the shared BMI088 driver remains separate because it currently hard-codes
`spi2`. Board Smoke was restored afterward with SHA-256
`781CD46356C55B9B397AB7AB8751EAF7B45CB1A6F92588681834C3C26930CDA4`.

## 2026-07-28 PWM C2 bounded observation

The operator observed four small servo movements from the demo-local PWM
sequence. This is
`PWM_C2_SERVO=BOUNDED_PHYSICAL_OBSERVATION_PASS` only. It does not establish
quantitative angle, pulse-width or load accuracy, and repository-wide
regression remains pending.
