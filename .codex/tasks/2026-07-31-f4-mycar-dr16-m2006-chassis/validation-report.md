---
document_type: engineering-validation-report
status: software-validated-hardware-gated
created: "2026-07-31"
last_updated: "2026-07-31"
task_slug: "f4-mycar-dr16-m2006-chassis"
project: "unmapped"
---

# Validation Report — F4 MyCar DR16 M2006 Chassis

## Final Tasks 1-6 software gate

Status: PASS for the Tasks 1-6 software gate after final integration
corrections, fresh validation and independent re-review. Hardware acceptance
remains a separate, unrun gate.

```text
repository: pnx_f4_mycar
branch: mycar/f4
baseline HEAD: ed9a2271371c61001fd7440f413b542c2ba64218
implementation: unstaged working-tree changes
host: 41/41 PASS
six F407 presets: PASS, compiler/linker warnings 0
static/graph/generated/symbol gate: PASS
hardware: NOT_RUN
same-cycle CAN send acceptance: UNKNOWN
```

### Host and review

- Fresh `build/final-safety-host` configure/build exited 0 with 49/49 Ninja steps
  and zero compiler/linker warnings.
- CTest exited 0 with 41/41 tests passing.
- Final cross-task code re-review found no remaining Critical or Important
  issue after the startup interlock, reachable reset, runtime-fault PI reset,
  malformed-manual, PI-config, telemetry and NaN-test corrections.

### Embedded builds

All six visible presets were configured and rebuilt with `--clean-first`:

| Preset | Result | RAM | Flash | SHA-256 |
| --- | --- | ---: | ---: | --- |
| `f407-debug` | 199/199, warnings 0 | 49,792 B | 23,736 B | `76533AB440EA66ECA26EA7F20781BD74D547B929038F0200F46596EB79E9A9AD` |
| `f407-release` | 199/199, warnings 0 | 49,792 B | 15,188 B | `AC9B2ECA96CEDE40C8F9B3705193347CBCE758EC4D22D4E412F6166488B89D91` |
| `f407-usb-cdc-debug` | 311/311, warnings 0 | 66,040 B | 67,952 B | `9BEE15E7687E97DA9BC62C45CAC63B452A551B6DEF9ACEDABA6E4613F5499847` |
| `f407-usb-cdc-release` | 311/311, warnings 0 | 66,000 B | 40,056 B | `95A1984239B22410D5D8B3936E01506F8E31B61877B2DC53035B8550ADA529A3` |
| `f407-pwm-a2-debug` | 200/200, warnings 0 | 49,864 B | 27,656 B | `B851F9DCDA2AA571892571DFEB1644BB58AEBB9A53084B744574BC5C4B191D99` |
| `f407-mycar-chassis-debug` | 215/215, warnings 0 | 55,368 B | 69,448 B | `D6A2EDD14562D15D0065B4FD71E3F17E3E502C77C70869DB57E2F3892EB5003E` |

Every ELF contains exactly one `app_start`. The five non-MyCar graphs and
ELFs contain no vehicle, direct CAN/USART, DR16/remoter or DJI-motor closure.
The MyCar graph contains the nine vehicle translation units once each, its
required CAN/USART/remoter/DJI sources, and retained runtime/thread/motor
symbols. Generated configuration reports DR16 on USART3 and four relaxed
M2006s on CAN1 IDs `0x201..0x204`.

Focused regressions prove that offline switch defaults cannot release the
startup interlock, a first fresh `up/up` sample cannot arm, health-loss reset
requires a fresh online release, runtime faults inhibit the controller and
reset PI, malformed manual samples fault closed, and ineffective integral-only
PI configuration is invalid. The runtime source contract also checks all four
overrun reset sites, one loop send and four one-shot registration sites; it is
a textual structural check, not an AST or hardware-runtime proof.

### Static and repository state

- The pure control-module allowlist from `types` through `controller` and
  configuration has zero HAL/ThreadX/BSP/message/remoter/motor dependency
  matches.
- `git diff --check` passes; staged file count is 0.
- Board/shared diff count is 0; all four submodules and the public
  `pnx_f4_minimal` worktree are clean.
- The excluded `demo/chassis_mecanum/` remains untracked and unmodified.
- No commit, push, remote change, Vault write, flash or hardware operation
  occurred.

### Open evidence gates

- Task 7 physical geometry, wheel/ID mapping and direction: NOT_RUN.
- Flash, live DR16/CAN, motor watchdog and zero-current bench: NOT_RUN.
- Non-zero current, PI tuning, single-wheel and lifted four-wheel tests:
  NOT_RUN.
- Ground motion, three repetitions, 120-second soak and measured safety
  latencies: NOT_RUN.
- WCET, deadline margin, stack high-water and critical-section duration:
  NOT_RUN.
- Same-cycle CAN transmit acceptance: UNKNOWN.
- Gearbox-output feedback equivalence to physical wheel speed: UNKNOWN.

The default MyCar configuration remains deliberately invalid and zero-only;
this report supports software readiness for measured bring-up, not a completed
or operational chassis.

## Baseline

```text
repository: pnx_f4_mycar
branch: mycar/f4
commit: ed9a2271371c61001fd7440f413b542c2ba64218
working tree: untracked demo/chassis_mecanum/
host compiler: Clang 22.1.8
hardware: NOT_RUN
```

Command:

```powershell
cmake -S . -B build/plan-baseline-host -G Ninja -DPNX_HOST_TESTS=ON
cmake --build build/plan-baseline-host
```

Observed:

- configure/generate: PASS;
- build: FAIL;
- cause: expected include files were absent because all four gitlinks were
  uninitialized in this worktree;
- chassis code validation: NOT_RUN;
- embedded build: NOT_RUN;
- flash/runtime/physical motion: NOT_RUN.

This historical baseline did not support a completion claim. It was resolved
by Task 1, which initialized and verified the exact pinned submodules before
implementation.
