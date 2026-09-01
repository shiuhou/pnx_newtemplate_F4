# MyCar F407 PS2 manual + vision-auto chassis/ARM

This repository is the vehicle-specific `mycar/f4` composition for a DJI
C-board (STM32F407) vehicle. The current competition product has two mutually
exclusive command sources:

```text
PS2 MANUAL: R1 chassis / R2 ARM -----------------------┐
MaixCam AUTO: USART6 AVC1 vx/vy + L1 dead-man --------┤
                                                       v
body_velocity -> slew -> X-mecanum -> four velocity PI
              -> bounded current -> four M2006 on CAN1
```

It is a vehicle repository, not a general chassis framework. Vehicle code
lives under `vehicle/`; the pinned `pnx_*` submodules and their public APIs
remain upstream-owned.

## Current status

The PS2 manual baseline is published on `chassis_x_arm`. This branch adds the
software-complete AVC1 vision command path and manual/AUTO arbitration. Host
tests and all affected F407 builds pass; this new AUTO ELF has not been
flashed or operated on hardware.

On 2026-08-04 the operator verified the hardware-accepted `DA37D784...` ELF:
three cold-start/re-arm cycles, DR16 loss with zero-output recovery, five
minutes of ground driving without creep or unexpected dropout, and correct
forward/backward, strafe and yaw directions. The later ownership cleanup keeps
the shared remoter axis contract standard and applies this vehicle's left-stick
axis swap in `vehicle/chassis`. Its `92691430...` ELF was then programmed,
verified and reset with OpenOCD; the operator confirmed re-arm, forward/backward,
strafe, yaw and centered stopping. This is a short exact-artifact hardware pass;
the extended five-minute and DR16-loss evidence still belongs to `DA37D784...`.

```text
Software gate: PASS
Hardware-accepted baseline ELF: PASS_OPERATOR_OBSERVED
Current ownership-cleanup ELF: FLASH_VERIFY_PASS, SHORT_HARDWARE_PASS
PS2 vision-auto software: PASS, HARDWARE_NOT_RUN
```

## Current control surface

- Boot, PS2 loss/reconnect and Cross are `locked + MANUAL`; Circle unlocks.
- In MANUAL, R1 keeps the existing chassis behavior and R2 keeps the existing
  ARM behavior.
- Square enters AUTO only after unlock. AUTO ignores R1/R2 and requires L1 to
  remain held.
- AUTO cannot move until a new AVC1 frame arrives after mode entry. `valid=0`,
  UART silence over 200 ms, queue overflow, L1 release, Triangle, Cross or any
  existing CAN/motor/config fault selects immediate zero and clears chassis
  slew/PI state.
- MaixCam owns recognition and follow-control math. The C board accepts only
  final `vx/vy`; automatic `wz` is always zero. See
  [the AVC1 interface contract](docs/vision-auto-chassis-interface.md).

## Start here

## 中文閱讀路線

本分支的最短新功能閱讀路線是：

1. [`docs/vision-auto-chassis-interface.md`](docs/vision-auto-chassis-interface.md)
   了解视觉组唯一需要遵守的 UART 契约。
2. `vehicle/combined/control/vision_command.*` 看 frame、stream queue、timeout
   与 AUTO gate。
3. `vehicle/combined/control/ps2_input_adapter.*` 看 Circle/Square/Triangle/
   Cross/L1 语义。
4. `vehicle/combined/runtime/runtime.cpp` 看命令仲裁如何进入同一个
   `vehicle/chassis/control/controller.*`。

不要由 STM32 HAL、CMSIS、ThreadX、USBX 或 `pnx_*` 子模組往上猜設計；它們分別是
第三方／上游實作。建議依下列順序閱讀目前 MyCar 路徑：

1. [`AGENTS.md`](AGENTS.md)：先確定車輛程式歸屬、fail-closed 與授權邊界。
2. [`CMakeLists.txt`](CMakeLists.txt) 與 `CMakePresets.json`：確認選的是
   `PNX_ENABLE_MYCAR_CHASSIS`，以及它實際編譯哪些 source。
3. [`configs/vehicles/mycar/params.json`](configs/vehicles/mycar/params.json) 與
   [`robot.json`](configs/vehicles/mycar/robot.json)：前者選 DR16／UART 等組態，後者
   綁定四個 M2006 的 CAN ID；JSON 不支援註釋，語意在本表與生成 CMake 中說明。
4. [`demo/app.cpp`](demo/app.cpp) → [`vehicle/mycar.cpp`](vehicle/mycar.cpp)：
   從共同入口進入 MyCar runtime 的最短呼叫鏈。
5. `vehicle/chassis/common/` → `control/` → `runtime/`：依資料型別、純控制、ThreadX/
   CAN/DR16 整合的方向閱讀。這是車輛功能的權威實作。
6. [`boards/dji_c_board_f407/README.md`](boards/dji_c_board_f407/README.md) 再配合
   `pnx_bsp/can/src/bsp_can.cpp`、`pnx_bsp/usart/src/bsp_usart.cpp`：只在需要
   追到 F407 HAL、generated handle 或腳位時才進入。
7. `tests/host/chassis_*`：將前述安全、運動學與控制規則當作可執行規格閱讀。

`demo/cboard/*`、USB、PWM、SPI、BMI088 與其他 demo 是獨立 closure 或歷史驗證；它們
可以用來了解 F407 基線，但不是本車 DR16/M2006 的執行鏈。CubeMX 生成檔與第三方資料夾
同理，只在追查底層問題時閱讀，不在車輛功能中直接修改。

For a fresh checkout of the published vehicle branch:

```powershell
git clone --branch mycar_f4 --recurse-submodules `
  https://github.com/shiuhou/pnx_newtemplate_F4.git pnx_f4_mycar
cd pnx_f4_mycar
```

Required tools are CMake 3.22+, Ninja, a native C++ compiler for Host tests,
and GNU Arm Embedded Toolchain for F407 builds.

Run the directly relevant Host suite:

```powershell
cmake -S . -B build/host -G Ninja -DPNX_HOST_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

Build the PS2 manual + vision-auto combined image:

```powershell
cmake --preset f407-mycar-combined-ps2-debug
cmake --build --preset f407-mycar-combined-ps2-debug
```

The output is `build/f407-mycar-combined-ps2-debug/pnx_embedded.elf`. Building
it is software evidence only; do not flash it or connect non-zero motor output
without separate authorization.

## Safety and authorization boundary

- The MyCar configuration records measured geometry and the attended-test
  limits, directions and P/current values. Invalid or non-finite values still
  fail-close to zero output.
- The controller requires a fresh online DR16 right-switch release before an
  UP rising edge can arm. Offline switch defaults cannot satisfy the startup
  interlock.
- Remote loss, motor/CAN health loss, invalid configuration, malformed manual
  input and timing overrun select relax/zero and reset PI state as applicable.
- Vision invalid/timeout/overflow and L1 release are recoverable stops, not new
  terminal fault latches. A later valid frame may resume only while every AUTO
  gate remains true.
- Hardware operation, flashing, non-zero current, commit, push and
  cross-workspace writes require explicit user authorization.

Treat the present gains and limits as a validated driveable baseline, not final
competition tuning. Re-measure and revalidate any higher-output configuration.

## Source and configuration map

| Need | Authoritative location |
| --- | --- |
| Vehicle control core and runtime adapter | `vehicle/chassis/common/`, `control/`, `runtime/` |
| Vehicle composition entry point | `vehicle/mycar.cpp` |
| Combined PS2/vision runtime | `vehicle/combined/runtime/runtime.cpp` |
| AVC1 parser, queue, snapshot and gate | `vehicle/combined/control/vision_command.*` |
| PS2 manual/AUTO mode input | `vehicle/combined/control/ps2_input_adapter.*` |
| Vision-team UART contract | `docs/vision-auto-chassis-interface.md` |
| Geometry, limits, directions and PI/current parameters | `vehicle/chassis/runtime/config.cpp` |
| DR16/UART build settings and four M2006 identities | `configs/vehicles/mycar/params.json`, `robot.json` |
| MyCar build selection | `CMakeLists.txt`, `CMakePresets.json` |
| Host behavior and regression tests | `tests/host/chassis_*` |
| F407 Direct BSP implementation | `pnx_bsp/*/src/` |
| CubeMX/generated handles, startup, linker and RTOS integration | `boards/dji_c_board_f407/` |

The intended data flow is:

```text
USART3 DR16 -> remoter service -> vehicle/chassis runtime
             -> wheel targets/current commands -> DJI motor handler -> CAN1
M2006 feedback 0x201..0x204 ------------------------------^
```

Wheel order is FL/FR/RL/RR. Coordinates are `+x` forward, `+y` left and
positive yaw counter-clockwise. The CAN IDs, motor directions and physical
forward/strafe/yaw response were verified on the vehicle on 2026-08-04.

## Human and AI-agent routing

Before changing anything, read [AGENTS.md](AGENTS.md). It defines repository
ownership, fail-closed rules, authorization boundaries and the default
STANDARD engineering mode.

Then use the smallest document set that answers the task:

1. This README for scope, commands and safety boundaries.
2. `vehicle/chassis/` and `configs/vehicles/mycar/` for an implementation or
   parameter question.
3. [HANDOFF.md](HANDOFF.md) for evidence history and current handover state.
4. The task packet under `.codex/tasks/2026-07-31-f4-mycar-dr16-m2006-chassis/`
   only when detailed validation provenance is needed.

Do not scan, modify or infer design from historical prototypes. Do not modify
shared `pnx_*` submodules or public APIs for a vehicle-only change.

## Next gated work

Task 7 is power-off/attended measurement of wheel radius, wheelbase, track,
wheel-to-CAN-ID mapping, positive directions, safe speed/current limits and
initial PI values.

Task 8 begins only with explicit authorization: flash, zero-current bench
checks, live DR16/CAN observation, one-wheel direction checks, lifted
four-wheel low-speed tests, ground tests, repeat runs and soak testing.

See [the implementation plan](docs/superpowers/plans/2026-07-31-f4-mycar-dr16-m2006-chassis.md)
and [the validation report](.codex/tasks/2026-07-31-f4-mycar-dr16-m2006-chassis/validation-report.md)
for the exact acceptance gates and evidence limits.

## Shared F407 baseline

This branch consumes the F407 Board/BSP foundation; it does not redefine that
foundation. For Board ownership, CubeMX rules and peripheral implementation,
read [the C-board README](boards/dji_c_board_f407/README.md). Public shared
contracts remain in the pinned `pnx_bsp`, `pnx_devices`, `pnx_libs` and
`pnx_modules` submodules.
