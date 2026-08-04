# MyCar F407 DR16 + four-M2006 chassis

This repository is the vehicle-specific `mycar/f4` composition for a DJI
C-board (STM32F407) chassis. It implements the first MyCar software slice:

```text
DR16 -> manual mapping -> X-mecanum inverse kinematics
     -> four velocity PI loops -> bounded raw current
     -> four M2006 motors on CAN1
```

It is a vehicle repository, not a general chassis framework. Vehicle code
lives under `vehicle/`; the pinned `pnx_*` submodules and their public APIs
remain upstream-owned.

## Current status

The software gate and attended chassis baseline acceptance have passed. The
current profile uses the measured chassis geometry, DR16 manual control, four
M2006 feedback loops, startup arming, health interlocks and bounded current.

On 2026-08-04 the operator verified the hardware-accepted `DA37D784...` ELF:
three cold-start/re-arm cycles, DR16 loss with zero-output recovery, five
minutes of ground driving without creep or unexpected dropout, and correct
forward/backward, strafe and yaw directions. The later ownership cleanup keeps
the shared remoter axis contract standard and applies this vehicle's left-stick
axis swap in `vehicle/chassis`; its newly built `92691430...` ELF has not yet
been reflashed. Exact-artifact hardware revalidation is therefore still a
release gate, not a software-development blocker.

```text
Software gate: PASS
Hardware-accepted baseline ELF: PASS_OPERATOR_OBSERVED
Current ownership-cleanup ELF: BUILD_PASS, HARDWARE_NOT_RUN
```

## Start here

## 中文閱讀路線

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

Build the MyCar image:

```powershell
cmake --preset f407-mycar-chassis-debug
cmake --build --preset f407-mycar-chassis-debug
```

The output is `build/f407-mycar-chassis-debug/pnx_embedded.elf`. Building it
is software evidence only; do not flash it or connect non-zero motor output
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
- Hardware operation, flashing, non-zero current, commit, push and
  cross-workspace writes require explicit user authorization.

Treat the present gains and limits as a validated driveable baseline, not final
competition tuning. Re-measure and revalidate any higher-output configuration.

## Source and configuration map

| Need | Authoritative location |
| --- | --- |
| Vehicle control core and runtime adapter | `vehicle/chassis/common/`, `control/`, `runtime/` |
| Vehicle composition entry point | `vehicle/mycar.cpp` |
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
