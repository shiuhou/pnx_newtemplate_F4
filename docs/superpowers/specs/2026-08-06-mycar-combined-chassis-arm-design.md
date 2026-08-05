# MyCar Combined Chassis and ARM Firmware Design

Date: 2026-08-06
Status: Approved for implementation

## Objective

Build and flash one STM32F407 image that controls the existing four-M2006
mecanum chassis and the complete J1-J4 arm from one DR16 receiver. All five
M2006 motors, the receiver, and the three PWM servos are connected to the same
DJI C-board.

The standalone chassis and ARM presets remain available. This change adds a
combined product; it does not change shared `pnx_*` interfaces or submodule
revisions.

## Operator Contract

| DR16 input | Combined behavior |
| --- | --- |
| `left_sw == up` | Chassis mode |
| `left_sw == mid` | Neutral mode |
| `left_sw == low` | ARM mode |
| `right_sw != up` | Global release/disarm |
| `right_sw == up` | Enable the selected mode after its existing interlock passes |
| Chassis: `left_y`, `left_x`, `right_x` | Forward/backward, strafe, yaw using the validated MyCar mapping |
| ARM: `right_y`, `left_y`, `right_x`, `left_x` | J1, J2, J3, J4 incremental control |

The physical DR16 wheel remains unused because hardware testing did not expose
a usable changing value.

## Selected Architecture

Add a dedicated combined product and runtime. The runtime is the sole owner of:

- remoter service initialization and the `remoter::state` subscription;
- the DR16 ingest thread and fresh snapshot;
- registration of chassis motors 1/2/4/3 and ARM J1 motor 5;
- the 20 ms motor feedback watchdog sample;
- the 200 Hz control deadline;
- the single DJI `send_control()` call for each normal cycle;
- J2-J4 PWM output objects and global fault arbitration.

The runtime reuses the existing chassis controller, chassis runtime policy,
ARM control primitives, ARM runtime policy, and servo PWM adapter. It must not
call the standalone `vehicle::mycar::run()` and `vehicle::arm::run()` functions,
because those functions independently own the same singleton services.

Alternative designs were rejected:

1. Starting both standalone runtimes would create competing remoter threads,
   watchdog calls, and CAN sends against singleton state.
2. Refactoring both standalone products into generic multi-client services
   would increase the change surface and risk the already validated products.
3. Changing shared submodules to support multiple runtime clients would move a
   vehicle-specific composition problem into public shared interfaces.

## Product Closure

Add `f407-mycar-combined-debug` with a mutually exclusive
`PNX_ENABLE_MYCAR_COMBINED` selector and `PNX_APP_MYCAR_COMBINED` entry.

The generated product configuration uses:

- the existing MyCar/ARM feature and DR16 parameters;
- one robot motor list containing `front_left`, `front_right`, `rear_left`,
  `rear_right`, and `j1` with their existing CAN buses and IDs;
- CAN, USART, and PWM BSP sources;
- both control implementations, but only the new combined hardware runtime.

No generated CubeMX files, IOC resource assignments, shared modules, or
standalone preset behavior are changed.

## Runtime Data Flow

Each 5 ms cycle performs these operations in order:

1. Copy the latest DR16 snapshot and invalidate it after 120 ms.
2. Read CAN telemetry and all five motor feedback values.
3. Every fourth cycle, call the global motor handler `alive_check()` once and
   capture the health of all five registered motors.
4. Evaluate both existing subsystem policies against the same observation.
5. Run the chassis controller only as an output source in chassis mode.
6. Run ARM manual control only in ARM mode. Preserve the existing J1 and PWM
   hold behavior after ARM has been armed while the remote remains healthy and
   `right_sw` remains up.
7. Apply global fault arbitration before selecting any motor or PWM output.
8. Set all five CAN motor commands, update J2-J4 PWM, and call
   `send_control()` once.
9. Publish one combined debugger telemetry snapshot.

## Mode and Output Rules

| State | Chassis M2006 | J1 M2006 | J2-J4 PWM |
| --- | --- | --- | --- |
| Chassis mode, enabled | Manual chassis control | Hold last ARM target if previously armed | Hold last commanded pulses if previously active |
| Neutral, enabled | Zero/relax | Hold last ARM target if previously armed | Hold last commanded pulses if previously active |
| ARM mode, enabled | Zero/relax | Manual ARM control | Manual ARM control |
| Released/disarmed | Zero/relax | Zero/relax | PWM stopped |
| Fault or stale remote | Zero/relax | Zero/relax | PWM stopped |

Mode selection alone never authorizes a new moving output. Existing release,
rising-edge, centered-axis, feedback, CAN, and configuration interlocks remain
in force. On every mode transition, the newly selected mode remains masked
until its control axes have returned to center. Chassis output is explicitly
masked outside chassis mode, even if its internal safety gate still reports an
earlier armed state.

## Fault Handling

The combined image is conservatively fail-closed as one product. Any of the
following latches a terminal combined runtime fault and removes all outputs
until reset/reboot:

- invalid chassis or ARM configuration;
- failure to register any of the five M2006 motors;
- remoter initialization, subscription, or thread creation failure;
- CAN health change that either existing policy rejects;
- PWM update/start/stop failure;
- 5 ms control deadline overrun.

Stale/offline/non-DR16 input or loss of any required motor feedback also
removes every output immediately, but remains a recoverable safety event.
After health returns, an explicit operator release and re-arm is required;
motion never resumes solely because feedback or radio data resumed.

Terminal runtime faults require diagnosis and reset/reboot; they are not
silently cleared while powered.

## Telemetry

Expose one combined `debug_state()` containing:

- selected mode, global fault mask, loop/overrun/remote counters, and CAN
  telemetry;
- chassis state, four targets, four measured speeds, and four currents;
- ARM state plus the existing J1 target/feedback/current/stall/hold fields;
- J2-J4 axes, pulse widths, and PWM enabled flags;
- per-product watchdog sampling and aggregate five-motor health.

## Verification and Flash Gate

Implementation starts with focused Host tests for mode routing, output
arbitration, combined source closure, and the union motor configuration. Then:

1. Run all Host CTest tests once before the implementation commit.
2. Configure and build `f407-mycar-combined-debug`.
3. Record ELF size and SHA-256.
4. With the chassis raised and `right_sw` at `mid`, program through the
   existing CMSIS-DAP/OpenOCD path and require `Verified OK` plus reset.
5. First observe neutral/no-motion behavior. Physical direction, transition,
   disconnect, and loaded tests remain operator-attended acceptance checks.

The parent repository will not be pushed without separate authorization.
