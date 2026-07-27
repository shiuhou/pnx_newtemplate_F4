# Vault Update Proposal

The Vault was not modified by this task.

## Current checkpoint proposal

Proposed durable statuses:

- record this path as the active local pure-F407 C-board workspace;
- record exact source/candidate/submodule provenance from `HANDOFF.md`;
- record the six preset builds as `RETAINED_PASS` and the previous host suite
  as `RETAINED_34_OF_34_PASS`;
- record the exact current combined-tree regression and expanded host suite as
  `NOT_RUN`;
- preserve `MULTIBOARD_F407_HW1=PASS_HISTORICAL` separately from
  the new `PURE_F407_HW1=PASS`;
- record the pure-F407 Board Smoke program/verify/reset, startup-chain,
  LED, ThreadX, DWT, stack, fault, and debugger-reset evidence;
- record the micro-USB-only power policy and isolated-lab development identity
  decision;
- retain the tracked USB descriptor fail-closed boundary;
- record isolated-lab USB CDC, bidirectional transfer and reconnect as
  `ISOLATED_LAB_PASS`, without assigning a production identity;
- record CAN1 and the bounded M2006 pulse as `PASS`;
- record PWM C2 as `BOUNDED_PHYSICAL_OBSERVATION_PASS`;
- record the replacement-board BMI088 and IST8310 results as `LAB_PASS`;
- record formal BMI088 + IST8310 common runtime as `NOT_DONE`;
- record UART as `SKIPPED_BY_USER_NOT_BLOCKING`, CAN2 as
  `NOT_RUN_OPTIONAL`, DBUS as `NOT_IMPLEMENTED_NOT_RUN`, MaixCam as
  `NOT_RUN`, and AHRS as `OUT_OF_SCOPE`;
- record daily integrated firmware as `PARTIAL` and team publication as
  `NOT_PUBLISHED`.

Review and import from a separate Vault task. This proposal is not evidence of
Vault synchronization.

## 2026-07-28 proposal delta

- record isolated-lab USB enumeration, CDC data/reopen/reset recovery as PASS;
- record CAN1 high-rate receive and M2006 zero-output as PASS;
- record the bounded `+500` / 250 ms M2006 actuation as PASS, including the
  operator-observed very short movement and automatic return to zero;
- record CAN2 as optional/not run and UART capture as skipped;
- retain DBUS as not implemented/not run;
- record restoration of the safe `board_smoke` image.

The Vault was still not modified.

## 2026-07-28 BMI088 proposal delta

- record BMI088 hardware validation as
  `BLOCKED_BY_BMI088_HARDWARE_NO_RESPONSE`, not PASS;
- record correct SPI1 pin/mode configuration, confirmed 24 V main power and
  measured 5 V rail;
- record accelerometer/gyro IDs `0xFF/0xFF` across SPI modes 0 through 3;
- retain local IMU power, solder/connectivity and sensor failure as unresolved
  physical suspects;
- record restoration of the safe Board Smoke image.

The Vault was not modified.

### IST8310 result

- record `STEER-PURE-F407-IST8310-MAG=PASS`;
- record ID `0x10`, data-ready `1`, fault/I2C error `0/0`;
- record magnetic sample change from `(2, -159, 168)` to
  `(-31, -174, -49)` after a 90-degree horizontal rotation;
- record restoration of Board Smoke.

The Vault was not modified.

### BMI088 replacement-board result

- retain the first board's all-`0xFF` result as board-specific negative
  evidence;
- record `STEER-PURE-F407-IMU-BMI088=PASS` on the replacement board;
- record IDs `0x1E/0x0F`, fault zero and gravity-axis movement from
  stationary `(74, -154, 5417)` to tilted `(-5133, 1532, 724)`;
- retain formal shared-driver SPI1 integration as separate follow-up work.

The Vault was not modified.

### PWM C2 result

- record four operator-observed small servo movements;
- classify the result only as a bounded physical observation;
- do not claim quantitative angle, pulse-width or load accuracy;
- retain repository-wide regression as pending.

The Vault was not modified.
