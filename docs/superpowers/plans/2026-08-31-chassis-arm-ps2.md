# Chassis + ARM PS2 Combined Image Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> `superpowers:executing-plans` task-by-task. Repository policy keeps
> `SUBAGENT=OFF` for this Standard-mode task.

**Goal:** Add an isolated PS2-controlled combined image without changing the
existing DR16 product or shared PS2 protocol.

**Architecture:** A compile-time product selector chooses one receiver source
and parameter file. A vehicle-local adapter converts PS2 buttons into existing
switch semantics while preserving `active_source=ps2`; mode routing,
subsystem controls, safety gates, and output arbitration remain downstream.

**Tech Stack:** C++17, CMake presets/contracts, Host CTest, STM32F407,
ThreadX, and existing `pnx_modules::remoter` APIs.

## Global constraints

- Baseline `806ee88`; branch `feat/ps2_chassisarm`.
- Do not modify submodules.
- Do not commit, push, flash, operate hardware, or write the Vault.
- Keep DR16 behavior and graph unchanged.
- Fail closed on stale/invalid PS2 state, Cross lock, and receiver reconnect.

### Task 1: Product selection and graph isolation

**Files:** `CMakeLists.txt`, `CMakePresets.json`,
`configs/vehicles/mycar_combined/params.ps2.json`, and
`tests/host/combined_product_contract.cmake`.

- [x] Add contract assertions for PS2 preset/config and exclusive receiver
  source selection; run the focused contract and observe failure.
- [x] Add the minimal combined-input selector, parameter file, and preset.
- [x] Rebuild and pass the focused Host contract.

### Task 2: Vehicle PS2 adapter

**Files:** create `vehicle/combined/control/ps2_input_adapter.hpp/.cpp` and
`tests/host/combined_ps2_input_adapter_tests.cpp`; modify
`tests/host/CMakeLists.txt`.

- [x] Write tests for locked Neutral startup, Circle unlock, Cross lock,
  R1/R2 held mode selection, shoulder conflict, offline/link loss, invalid
  axes, reconnect reset, and DR16 pass-through; observe the old-behavior
  failures.
- [x] Implement the smallest stateful adapter satisfying those tests.
- [x] Build and run the adapter target.

### Task 3: Trusted-source and axis policies

**Files:** `vehicle/combined/control/mode_router.cpp`,
`vehicle/chassis/runtime/runtime_policy.cpp`,
`vehicle/arm/runtime/arm_runtime_policy.cpp`, and their Host tests.

- [x] Add failing tests accepting connected PS2, rejecting disconnected PS2,
  preserving DR16 behavior, mapping PS2 right stick to translation and left-X
  to yaw, preserving all ARM axes, and allowing Circle-then-R2 ARM arming.
- [x] Implement local trusted-source predicates and source-specific chassis
  mapping without changing shared APIs.
- [x] Run the three focused policy targets.

### Task 4: Combined runtime integration and telemetry

**Files:** `vehicle/combined/runtime/runtime.cpp/.hpp` and
`tests/host/combined_runtime_source_contract.cmake`.

- [x] Require adapter-before-router ordering, selected PS2 configuration, and
  telemetry fields in the source contract; observe failure.
- [x] Configure only the selected receiver, invoke the adapter before routing,
  and publish source/link/button/mode/unlock/R1/R2 telemetry.
- [x] Run the focused runtime contract.

### Task 5: Full software verification and handoff

- [x] Run focused adapter/router/chassis/ARM/runtime tests.
- [x] Run the complete Host suite once.
- [x] Fresh build PS2 and DR16 combined presets.
- [x] Inspect Ninja graphs for receiver isolation and forbidden H7, DM IMU,
  referee, TIM6, and heating-lease sources.
- [x] Review `git diff --check`, submodule cleanliness, and full diff.
- [x] Update `HANDOFF.md`, `VAULT_UPDATE.md`, and task validation with observed
  facts; leave all changes uncommitted for operator review.
