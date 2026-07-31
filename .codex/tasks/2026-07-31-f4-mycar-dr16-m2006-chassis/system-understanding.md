---
document_type: engineering-system-understanding
status: software-implemented-hardware-gated
created: "2026-07-31"
last_updated: "2026-07-31"
task_slug: "f4-mycar-dr16-m2006-chassis"
project: "unmapped"
---

# System Understanding — F4 MyCar DR16 M2006 Chassis

## Context

```text
repository: pnx_f4_mycar
branch: mycar/f4
head: ed9a2271371c61001fd7440f413b542c2ba64218
working tree: unstaged Tasks 1-6 changes plus preserved ?? demo/chassis_mecanum/
staged state: empty
vault project: unmapped; Vault not modified
```

## Data and control flow

```text
USART3 RX DMA -> remoter::service -> remoter::state
 -> vehicle manual mapping -> body velocity (+x forward, +y left, +yaw CCW)
 -> X-mecanum inverse kinematics -> FL/FR/RL/RR target rad/s
 -> four explicit-dt PI loops -> bounded raw currents
 -> motors::djimotorhandler -> CAN 0x200
 -> M2006 feedback 0x201..0x204 -> measured rad/s -> PI
```

## Safety flow

```text
invalid config / remote offline / switches not armed / motor offline /
CAN fault / control overrun
 -> reset all PI state -> relax all motors -> zero-current control frame
 -> controller health-loss reset requires deliberate switch release;
    runtime config/CAN/overrun faults remain sticky for the boot
```

Only fresh online remote samples update release/edge history, so offline
default switch values cannot satisfy the startup interlock. A non-finite
manual sample is treated as invalid configuration at the controller boundary
and fault-latches an armed controller.

Pure control modules from `types` through `controller` know no HAL, BSP,
ThreadX, CAN or message type. The Host-testable runtime policy consumes only
normalized remoter state and CAN telemetry types; `runtime.cpp` owns the
ThreadX/message/motor adapters. Shared modules remain pinned and unchanged.

The runtime uses a separate remote-ingest thread so the 200 Hz loop never
calls blocking `msg::read`. A successful snapshot remains usable for at most
120 ticks; snapshot copy occurs before the current-time sample to avoid a
priority/tick race. The default configuration remains invalid and zero-only.
