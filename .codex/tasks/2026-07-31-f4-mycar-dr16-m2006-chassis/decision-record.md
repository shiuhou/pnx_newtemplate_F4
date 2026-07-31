---
document_type: engineering-decision-record
status: implemented-software-scope
created: "2026-07-31"
last_updated: "2026-07-31"
decision_id: "DR-001"
task_slug: "f4-mycar-dr16-m2006-chassis"
project: "unmapped"
---

# Decision Record — Vehicle-local chassis without pnx_application

## Decision

Implement the first chassis under `vehicle/chassis` in the `mycar/f4`
vehicle worktree. Use pure vehicle-local C++ control modules plus one thin
ThreadX/DR16/CAN/M2006 adapter. Do not create `pnx_application` and do not
change a shared submodule interface.

## Alternatives

| Candidate | Result |
|---|---|
| Put chassis in the public F4 worktree | rejected; mixes car behavior with the reusable architecture |
| Add a generic `pnx_application` layer | rejected for v1; no demonstrated cross-car contract |
| Vehicle-local pure core plus runtime adapter | selected; testable, bounded and reversible |

## Consequences

Car geometry, direction, limits and tuning stay vehicle-owned. If a missing
shared capability is discovered, implementation stops and opens a separate
shared-contract decision instead of patching a gitlink opportunistically.

The user's selection of `pnx_f4_mycar`, DR16 manual closed loop and four M2006s
is the approval evidence for this scope. It is not authorization for non-zero
hardware output, commit or push.

Tasks 1-6 implement this decision and passed independent software review.
Physical configuration and hardware acceptance remain separate open gates.
