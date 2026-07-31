---
document_type: engineering-source-map
status: software-implemented
created: "2026-07-31"
last_updated: "2026-07-31"
task_slug: "f4-mycar-dr16-m2006-chassis"
project: "unmapped"
---

# Source Map — F4 MyCar DR16 M2006 Chassis

External research is not required for the software plan. Current repository
contracts and the user-provided motor/control scope determine v1. Manufacturer
documentation becomes required before raising current, speed, duty or thermal
limits beyond the separately approved low-energy bring-up envelope.

| Source | Version/locator | Supports | Limit |
|---|---|---|---|
| `pnx_f4_mycar` | `mycar/f4@ed9a227` plus unstaged vehicle changes | board, CMake, IOC and product baseline | no commit or push |
| `boards/dji_c_board_f407/dji_c_board_f407.ioc` | current worktree | CAN1 1 Mbps, USART3 RX DMA, DR16 line settings, 1000 Hz ThreadX tick | configuration presence is not runtime proof |
| pinned `pnx_devices` | initialized clean gitlink `2349cc1` | M2006 class, rad/s feedback and DJI handler contracts | same-cycle handler transmit acceptance is unavailable |
| pinned `pnx_modules` | initialized clean gitlink `8ba925b` | normalized DR16 state and offline flag | live DR16 remains to be verified in this worktree |
| user statement | 2026-07-31 | four M2006 and DR16 manual closed loop | geometry/directions/limits not supplied |

Historical examples and `demo/chassis_mecanum/` are excluded as load-bearing
sources.

The final software gate used repository contracts only. No external research
or prototype value was introduced.
