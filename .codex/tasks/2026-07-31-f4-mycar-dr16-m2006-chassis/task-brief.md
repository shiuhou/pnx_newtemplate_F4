---
document_type: engineering-task-brief
status: software-implemented-hardware-gated
created: "2026-07-31"
last_updated: "2026-07-31"
task_slug: "f4-mycar-dr16-m2006-chassis"
project: "unmapped"
---

# Task Brief — F4 MyCar DR16 M2006 Chassis

## Objective

Build a vehicle-local, testable and fail-closed control path:

```text
DR16 -> manual mapping -> mecanum inverse kinematics
     -> four velocity PI loops -> bounded raw current -> four M2006 on CAN1
```

## Deliverables

- Pure C++ kinematics, manual mapping, safety gate and velocity PI controller.
- Vehicle-local ThreadX/DR16/CAN/M2006 runtime adapter.
- One explicit `f407-mycar-chassis-debug` profile.
- Host, build, closure-isolation and staged hardware validation.

## Fixed constraints

- Target only `pnx_f4_mycar`, branch `mycar/f4`.
- Four M2006, first version DR16 manual closed loop.
- No `pnx_application`; vehicle code is under `vehicle/chassis`.
- No shared submodule/API change.
- Existing untracked `demo/chassis_mecanum/` is preserved and excluded.
- Product parameters and current output remain zero until measured/authorized.
- No commit, push, remote change, CubeMX regeneration or hardware operation.

## Acceptance

Software acceptance requires all Host tests, Core regression and car build to
pass. Hardware acceptance is separate and requires explicit authorization,
wheel-off-ground direction tests, low-speed closed-loop evidence, safety
timeouts, three ground repetitions and a 120-second soak.

Software acceptance is now PASS for Tasks 1-6: Host 41/41, six clean F407
preset builds warning-free, isolated compile graphs and approved reviews.
Hardware acceptance remains NOT_RUN.

## Unknowns and gates

Wheel geometry, physical direction signs, wheel-to-ID mapping and controller
gains are unknown. They are resolved by the exact power-off and attended
procedures in the implementation plan; zero sentinels prevent guessed values
from enabling motion.
