---
document_type: engineering-task-packet-readme
status: software-validated-hardware-gated
created: "2026-07-31"
last_updated: "2026-07-31"
task_slug: "f4-mycar-dr16-m2006-chassis"
project: "unmapped"
---

# Engineering Task Packet — F4 MyCar DR16 M2006 Chassis

This packet records the evidence, decisions, plan and future validation for
the first four-M2006 mecanum chassis version in `pnx_f4_mycar`.

Canonical workflow:
`C:\Users\USER\Desktop\xnotes\docs\ENGINEERING_TASK_WORKFLOW.md`

The executable plan is:
`docs/superpowers/plans/2026-07-31-f4-mycar-dr16-m2006-chassis.md`.

Tasks 1-6 now have reviewed source, Host, cross-build, graph, generated-config
and ELF evidence. Tasks 7-8 remain gated: no physical parameter measurement,
flash, live DR16/CAN observation, non-zero output or ground motion has been
performed. The implementation is still uncommitted, and the Vault has not
been modified.
