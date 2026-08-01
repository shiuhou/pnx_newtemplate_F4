# MyCar F407 DR16 + four-M2006 chassis

This repository is the vehicle-specific `mycar/f4` composition for a DJI
C-board (STM32F407) chassis. It implements the first MyCar software slice:

```text
DR16 -> manual mapping -> X-mecanum inverse kinematics
     -> four velocity PI loops -> bounded raw current
     -> four M2006 motors on CAN1
```

It is a vehicle repository, not a general chassis framework. Vehicle code
lives under `vehicle/`; the pinned `pnx_*` submodules and their public APIs
remain upstream-owned.

## Current status

The Tasks 1-6 software gate has passed. The implementation has Host tests,
an explicit MyCar F407 preset, generated four-motor configuration, safety
interlocks, and static/runtime-policy validation.

Hardware acceptance has **not** been run. The default MyCar parameters are
deliberately invalid and zero-only until the real vehicle is measured and
explicitly authorized. A successful build does not authorize flashing,
physical motor output, PI tuning, or driving.

```text
Software gate: PASS
Task 7 physical parameters: NOT_RUN
Task 8 flash and hardware acceptance: NOT_RUN
Same-cycle CAN send acceptance: UNKNOWN
```

## Start here

For a fresh checkout of the published vehicle branch:

```powershell
git clone --branch mycar_f4 --recurse-submodules `
  https://github.com/shiuhou/pnx_newtemplate_F4.git pnx_f4_mycar
cd pnx_f4_mycar
```

Required tools are CMake 3.22+, Ninja, a native C++ compiler for Host tests,
and GNU Arm Embedded Toolchain for F407 builds.

Run the directly relevant Host suite:

```powershell
cmake -S . -B build/host -G Ninja -DPNX_HOST_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Build the MyCar image:

```powershell
cmake --preset f407-mycar-chassis-debug
cmake --build --preset f407-mycar-chassis-debug
```

The output is `build/f407-mycar-chassis-debug/pnx_embedded.elf`. Building it
is software evidence only; do not flash it or connect non-zero motor output
without separate authorization.

## Safety and authorization boundary

- The shipped MyCar configuration has zero sentinel values for unmeasured
  geometry, direction, limits, PI gains and current limits. It therefore
  fail-closes to zero output.
- The controller requires a fresh online DR16 release before arming. Offline
  switch defaults cannot satisfy the startup interlock.
- Remote loss, motor/CAN health loss, invalid configuration, malformed manual
  input and timing overrun select relax/zero and reset PI state as applicable.
- Hardware operation, flashing, non-zero current, commit, push and
  cross-workspace writes require explicit user authorization.

Do not replace unmeasured values with guesses merely to make the chassis move.

## Source and configuration map

| Need | Authoritative location |
| --- | --- |
| Vehicle control core and runtime adapter | `vehicle/chassis/` |
| Vehicle composition entry point | `vehicle/mycar.cpp` |
| Geometry, limits, directions and PI/current parameters | `configs/vehicles/mycar/params.json` |
| DR16 and four M2006 identities | `configs/vehicles/mycar/robot.json` |
| MyCar build selection | `CMakeLists.txt`, `CMakePresets.json` |
| Host behavior and regression tests | `tests/host/chassis_*` |
| F407 Board/HAL implementation | `boards/dji_c_board_f407/` |

The intended data flow is:

```text
USART3 DR16 -> remoter service -> vehicle/chassis runtime
             -> wheel targets/current commands -> DJI motor handler -> CAN1
M2006 feedback 0x201..0x204 ------------------------------^
```

Wheel order is FL/FR/RL/RR. Coordinates are `+x` forward, `+y` left and
positive yaw counter-clockwise. Motor feedback is treated as signed
gearbox-output angular velocity; equivalence to physical wheel speed still
requires mechanical verification.

## Human and AI-agent routing

Before changing anything, read [AGENTS.md](AGENTS.md). It defines repository
ownership, fail-closed rules, authorization boundaries and the default
STANDARD engineering mode.

Then use the smallest document set that answers the task:

1. This README for scope, commands and safety boundaries.
2. `vehicle/chassis/` and `configs/vehicles/mycar/` for an implementation or
   parameter question.
3. [HANDOFF.md](HANDOFF.md) for evidence history and current handover state.
4. The task packet under `.codex/tasks/2026-07-31-f4-mycar-dr16-m2006-chassis/`
   only when detailed validation provenance is needed.

Do not scan, modify or infer design from historical prototypes. Do not modify
shared `pnx_*` submodules or public APIs for a vehicle-only change.

## Next gated work

Task 7 is power-off/attended measurement of wheel radius, wheelbase, track,
wheel-to-CAN-ID mapping, positive directions, safe speed/current limits and
initial PI values.

Task 8 begins only with explicit authorization: flash, zero-current bench
checks, live DR16/CAN observation, one-wheel direction checks, lifted
four-wheel low-speed tests, ground tests, repeat runs and soak testing.

See [the implementation plan](docs/superpowers/plans/2026-07-31-f4-mycar-dr16-m2006-chassis.md)
and [the validation report](.codex/tasks/2026-07-31-f4-mycar-dr16-m2006-chassis/validation-report.md)
for the exact acceptance gates and evidence limits.

## Shared F407 baseline

This branch consumes the F407 Board/BSP foundation; it does not redefine that
foundation. For Board ownership, CubeMX rules and peripheral implementation,
read [the C-board README](boards/dji_c_board_f407/README.md). Public shared
contracts remain in the pinned `pnx_bsp`, `pnx_devices`, `pnx_libs` and
`pnx_modules` submodules.
