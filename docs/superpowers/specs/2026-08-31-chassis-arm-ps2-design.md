# Chassis + ARM PS2 Combined Image Design

## Status

Approved by the operator on 2026-08-31 for local implementation on
`feat/ps2_chassisarm`, based on `chassis_x_arm@806ee88`.

## Goal and boundaries

Add a separately selectable STM32F407 combined image controlled by the
existing `pnx_modules::ps2` receiver path while preserving the validated DR16
combined image unchanged as a rollback target.

- Do not change the `pnx_modules` PS2 protocol or public API.
- Preserve chassis, ARM, motor/servo controls, and the global output arbiter.
  Keep the ARM safety gate's DR16 release rule while admitting the PS2 global
  locked sample as a source-specific release qualification.
- Translate PS2 state into existing vehicle switch/axis semantics before the
  mode router and subsystem policies.
- Compile exactly one receiver implementation per image: DR16 uses
  `dr16.cpp`; PS2 uses `ps2.cpp`.
- Preserve `remoter::state::active_source == ps2`; never impersonate DR16.
- Do not commit, push, flash, operate hardware, edit submodules, or write the
  Vault without separate authorization.

## Product composition

The existing `f407-mycar-combined-debug` remains the DR16 product. New preset
`f407-mycar-combined-ps2-debug` selects the same combined runtime, five-motor
robot configuration, chassis/ARM controls, and arbiter, but uses
`configs/vehicles/mycar_combined/params.ps2.json`, USART1, and `ps2.cpp`.

PS2 parameters are source `ps2`, USART1, priority 2, receiver-offline timeout
600 ticks, frame timeout 20 ticks, and deadzone 0.08.

## Input translation

`vehicle::combined::ps2_input_adapter` is stateful only for the unlock latch and
passes fresh DR16 input through unchanged. For fresh connected PS2 input:

- Circle pressed edge latches unlocked (`right_sw=up`).
- Cross pressed edge locks immediately (`right_sw=mid`).
- While unlocked, R1 held exclusively selects chassis (`left_sw=up`).
- While unlocked, R2 held exclusively selects ARM (`left_sw=low`).
- Both or neither shoulder maps to Neutral (`left_sw=mid`); Square is unused.
- Right Y, Left Y, Right X, Left X remain J1, J2, J3, J4 in ARM mode.

Startup is Neutral/locked. Circle must first be observed released, preventing a
held button from unlocking at startup or reconnect. Offline input,
non-connected PS2 link, non-finite or out-of-range axes, or a wrong source
resets the adapter, clears axes, marks the translated state offline, and
prevents output. Reconnect cannot restore unlock authority. If Circle and
Cross edges conflict, Cross wins and the adapter remains locked.

## Trusted sources and mappings

The mode router and both policies trust only fresh DR16 or fresh connected
PS2. All other sources fail closed. DR16 preserves its vehicle-specific
mapping. This receiver reports physical right-stick vertical as `right_y` and
horizontal as `right_x`; based on the latest hardware observation, PS2 chassis
maps receiver `left_x -> vx`, `left_y -> vy`, and `right_x -> yaw`. ARM axes remain
unchanged for both. Chassis entry checks the PS2
right stick and left-X yaw axis are centered before enabling output.

## Runtime, telemetry, and verification

The combined loop reads one fresh state, applies the adapter, routes mode,
updates both policies, and uses the existing output arbiter. Telemetry adds
active source, PS2 link, held/pressed buttons, routed mode, unlock latch, and
R1/R2 held state.

Host tests cover startup, lock/unlock edges, shoulder conflicts, link loss,
invalid axes, reconnect, DR16 pass-through, trusted-source policies, mapping,
PS2 Circle-then-R2 ARM arming, and runtime ordering. Fresh PS2/DR16 builds must
prove receiver isolation and
exclude H7, DM IMU, referee, TIM6, and heating-lease closures. Hardware work
is deferred and requires mechanical support because disarm removes J1 hold.
