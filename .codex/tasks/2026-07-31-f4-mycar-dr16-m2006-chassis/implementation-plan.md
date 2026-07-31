---
document_type: engineering-implementation-plan
status: software-tasks-1-6-complete
created: "2026-07-31"
last_updated: "2026-07-31"
task_slug: "f4-mycar-dr16-m2006-chassis"
project: "unmapped"
---

# Implementation Plan — F4 MyCar DR16 M2006 Chassis

The decision-complete executable plan is:

`docs/superpowers/plans/2026-07-31-f4-mycar-dr16-m2006-chassis.md`

Stages are: baseline/submodule gate, pure kinematics, DR16 mapping and safety,
explicit-dt velocity PI, fail-closed car build graph, ThreadX/M2006 runtime,
measured configuration, zero-current bench, one-wheel direction checks,
four-wheel closed loop and low-speed ground acceptance.

Every stage has a focused test, stop condition and rollback point. Commit,
push, hardware and non-zero motor output remain separate approval gates.

Execution status on 2026-07-31: Tasks 1-6 passed their software gates and
independent reviews. Task 7 awaits measured vehicle parameters; Task 8 awaits
explicit flash/hardware/non-zero-output authorization.
