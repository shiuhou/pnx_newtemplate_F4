# Chassis + ARM PS2 task

- Objective: add an isolated PS2 combined image through a vehicle-level input
  adapter while preserving the DR16 image and shared PS2 protocol.
- Baseline: `chassis_x_arm@806ee88`; Host `57/57 PASS`; DR16 combined fresh
  build PASS at RAM `60,920 B`, Flash `98,640 B`.
- Branch: `feat/ps2_chassisarm`.
- Mode: STANDARD, single agent, concise task record.
- Current status: local uncommitted PS2 stick-remap correction complete; latest
  ELF is flashed and verified. Operator direction validation remains.
- Prohibited without separate authorization: commit, push, submodule
  modification, Vault write. Hardware operation is authorized for this test.
