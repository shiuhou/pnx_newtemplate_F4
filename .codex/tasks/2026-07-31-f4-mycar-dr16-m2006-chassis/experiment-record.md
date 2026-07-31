---
document_type: engineering-experiment-record
status: software-only-complete-hardware-not-run
created: "2026-07-31"
last_updated: "2026-07-31"
task_slug: "f4-mycar-dr16-m2006-chassis"
project: "unmapped"
---

# Experiment Record — F4 MyCar DR16 M2006 Chassis

No hardware experiment has been run for this task.

The initial native Host baseline configured and then failed compilation because
the car worktree's pinned submodules were not initialized. After the exact
gitlinks were initialized, software experiments proceeded test-first:

- focused RED/GREEN tests for kinematics, manual/safety, controller,
  fail-closed configuration and runtime policy;
- regression RED/GREEN for atomic controller rejection, remote snapshot/time
  ordering, generated `relax` mode and `NDEBUG`-independent checks;
- final integration RED/GREEN for valid-sample-only startup interlock,
  release-gated controller reset, runtime-fault controller inhibition,
  overrun PI reset, malformed manual input, ineffective PI configuration,
  post-publication telemetry and NaN-safe kinematics assertions;
- final fresh Host build and CTest: 49/49 build steps, 41/41 tests, warnings 0;
- clean-first builds of all six F407 presets: PASS, warnings 0;
- compile graph, generated configuration, ELF symbol, pure dependency,
  whitespace and repository-state checks: PASS.

These are software experiments only. Firmware images were produced but were
not flashed or executed. No DR16 frame, CAN feedback, watchdog result, motor
current, wheel motion, timing margin, stack high-water or physical observation
was collected. Same-cycle CAN send acceptance remains UNKNOWN because the
pinned handler returns `void`.

Future hardware procedures and stop conditions are defined in Task 8 of the
implementation plan.
