---
document_type: engineering-evidence-map
status: software-validated-hardware-gated
created: "2026-07-31"
last_updated: "2026-07-31"
task_slug: "f4-mycar-dr16-m2006-chassis"
project: "unmapped"
---

# Evidence Map — F4 MyCar DR16 M2006 Chassis

| Claim | Class | Evidence | Status/impact |
|---|---|---|---|
| Target is `pnx_f4_mycar`, branch `mycar/f4` | Verified fact | `git branch --show-current` | confirmed |
| Planning HEAD is `ed9a2271371c61001fd7440f413b542c2ba64218` | Verified fact | `git rev-parse HEAD` | confirmed |
| Pre-existing `demo/chassis_mecanum/` remains untracked | Verified fact | scoped `git status --short -- demo/chassis_mecanum` | preserved and excluded |
| Four pinned gitlinks are initialized and clean | Verified fact | `git submodule status`; four porcelain counts are 0 | software build baseline established |
| v1 uses DR16 manual closed loop and four M2006s | User-provided constraint | user message, 2026-07-31 | fixed scope |
| CAN1 is configured for 1 Mbps | Verified fact | IOC `CAN1.CalculateBaudRate=1000000` | static configuration only |
| DR16-capable USART3 RX DMA exists | Verified fact | IOC USART3/DMA entries | static configuration only |
| ThreadX tick is 1000 Hz | Verified fact | IOC and `tx_user.h` | makes 5 ticks = 5 ms |
| Wheel radius, wheelbase and track are known | Unknown | no current measurement | must remain zero sentinel |
| IDs `0x201..0x204` match FL/FR/RL/RR physical wheels | Assumption | planned device tree | power-off/live feedback gate |
| Product direction signs are all `+1` | Assumption | safe initial config only | must be verified one wheel at a time |
| Current limit and PI gains are safe | Unknown | no current experiment | remain zero until approval/tuning |
| Pure chassis behavior passes Host tests | Software test | final CTest | 41/41 PASS overall |
| Six visible F407 profiles compile and link | Cross-build | clean-first preset builds | 6/6 PASS, warnings 0 |
| Core/USB/PWM exclude MyCar/CAN/USART/DR16/M2006 closure | Static build graph | six compile databases and ELFs | PASS |
| MyCar generated config has DR16/USART3 and four relaxed M2006s | Generated contract | config/robot headers and Host contract | PASS |
| MyCar default can drive the vehicle | Software configuration | zero sentinels and `valid()` | false; zero-only by design |
| Same-cycle CAN frame was accepted | Runtime observation | handler returns `void`; no hardware run | UNKNOWN |
| Flash, live DR16/CAN, wheel motion and safety timing work | Hardware observation | no authorized hardware run | NOT_RUN |

The initial missing-header failure remains useful baseline evidence: it was
caused by uninitialized gitlinks, not chassis code. After initializing the
exact pins, Tasks 1-6 completed with fresh Host, build, graph, generated-output
and independent review evidence. Physical facts remain unresolved rather than
being inferred from those software results.
