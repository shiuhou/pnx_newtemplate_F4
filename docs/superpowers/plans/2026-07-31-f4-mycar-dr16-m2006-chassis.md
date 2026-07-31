# F4 MyCar DR16 M2006 Chassis Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `pnx_f4_mycar` 建立首版四麥輪底盤：DR16 手動命令經麥克納姆逆解算與四路速度 PI 閉環，驅動 CAN1 上四個 M2006，並在遙控器、電機或 CAN 異常時 fail-closed 為零電流。

**Architecture:** 車輛專屬程式放在 `vehicle/chassis`，不建立 `pnx_application`，也不修改 `pnx_bsp`、`pnx_devices`、`pnx_libs` 或 `pnx_modules` 的公共介面。座標解算、DR16 命令映射、安全狀態機與 PI 控制器保持 HAL/ThreadX-free，使用 native Host tests 驗證；`runtime.cpp` 才負責 ThreadX、`remoter::service`、M2006 與 CAN adapter。

**Tech Stack:** C++17、CMake 3.22+、Ninja、CTest、GNU Arm Embedded Toolchain、STM32F407、ThreadX 1000 ticks/s、PnX public BSP/Device/Module contracts、DJI M2006/C610 classic CAN。

## Global Constraints

- 唯一實作 worktree：`C:\Users\USER\Desktop\RM\rm校內賽\2026\firmware\pnx_f4_mycar`，branch `mycar/f4`，規劃基線 `ed9a2271371c61001fd7440f413b542c2ba64218`。
- `pnx_f4_minimal` 公共 worktree只作讀取參考；不得修改。
- 現有未追蹤 `demo/chassis_mecanum/` 不讀作設計基準、不修改、不刪除、不納入提交。
- 首版電機固定為四個 `motors::m2006`，CAN1 feedback IDs 固定配置為 `0x201`、`0x202`、`0x203`、`0x204`，control frame 為既有 handler 的 `0x200`。
- 底盤座標固定為 `+x` 前、`+y` 左、`+yaw` 逆時針；輪序固定為 FL、FR、RL、RR。
- 控制週期固定 5 ThreadX ticks，即 200 Hz；目前 board 設定 `TX_TIMER_TICKS_PER_SECOND=1000`。
- DR16 來源固定 USART3 RX DMA，100000 baud、9-bit word length/even parity（等效 8E1），由既有 IOC 與 `remoter` module 管理。
- 正常 Core、USB 與 PWM images 不得取得 CAN、USART、DR16、M2006 或車輛 source closure。
- 車輛參數的 wheel radius、half wheelbase、half track、速度上限、PI gains 與非零電流上限初始值均為 `0`；`valid()` 必須拒絕啟動，因此初始 car image 只可發送零電流。
- 非零電機輸出、燒錄、上電與實機測試均需另外取得使用者明確授權。
- 不修改 IOC/CubeMX generated source，不安裝 production dependency，不修改 remote，不 commit、不 push。

---

## Public interfaces and file map

新增：

```text
vehicle/
├─ mycar.hpp / mycar.cpp                  車輛 composition root
└─ chassis/
   ├─ types.hpp                           座標、輪序與純資料型別
   ├─ config.hpp / config.cpp             車輛參數與 fail-closed 驗證
   ├─ kinematics.hpp / kinematics.cpp     麥克納姆逆解算與等比例飽和
   ├─ manual_control.hpp / .cpp            DR16 正規化輸入映射
   ├─ safety_gate.hpp / .cpp               解鎖、離線與 fault latch
   ├─ velocity_pi.hpp / .cpp               顯式 dt、anti-windup 速度 PI
   ├─ controller.hpp / .cpp                純控制鏈整合
   └─ runtime.hpp / runtime.cpp             ThreadX、消息、CAN、M2006 adapter
```

核心型別與簽名：

```cpp
namespace vehicle::chassis {

enum class wheel : std::uint8_t { front_left, front_right, rear_left, rear_right };

struct body_velocity {
    float vx_mps;
    float vy_mps;
    float yaw_rad_s;
};

struct wheel_vector {
    std::array<float, 4> rad_s;
};

struct geometry {
    float wheel_radius_m;
    float half_wheelbase_m;
    float half_track_m;
    float max_wheel_rad_s;
};

std::optional<wheel_vector> inverse_kinematics(
    const body_velocity& command, const geometry& geometry) noexcept;

struct manual_input {
    bool online;
    bool arm_switches_up;
    float left_x;
    float left_y;
    float right_x;
};

struct manual_limits {
    float deadband;
    float max_vx_mps;
    float max_vy_mps;
    float max_yaw_rad_s;
    float vx_sign;
    float vy_sign;
    float yaw_sign;
};

body_velocity map_manual(
    const manual_input& input, const manual_limits& limits) noexcept;

enum class safety_state : std::uint8_t {
    disabled, waiting_remote, waiting_motors, armed, fault_latched
};

struct safety_input {
    bool remote_online;
    bool arm_switches_up;
    bool all_motors_online;
    bool can_healthy;
    bool config_valid;
};

class safety_gate {
public:
    safety_state update(const safety_input& input) noexcept;
    bool output_enabled() const noexcept;
    void reset() noexcept;
};

struct velocity_pi_config {
    float kp;
    float ki_per_s;
    float integral_limit_raw;
    float current_limit_raw;
};

class velocity_pi {
public:
    explicit velocity_pi(velocity_pi_config config) noexcept;
    std::int16_t update(float target_rad_s, float measured_rad_s, float dt_s) noexcept;
    void reset() noexcept;
};

} // namespace vehicle::chassis
```

## Task 1: Establish the car-worktree baseline and ownership boundary

**Files:**

- Modify: `AGENTS.md`
- Modify: `README.md`
- Preserve: `demo/chassis_mecanum/**`

**Interfaces:**

- Consumes: Git branch `mycar/f4` and the four gitlinks recorded by `ed9a227`.
- Produces: a documented vehicle-branch boundary; no firmware behavior change.

- [ ] **Step 1: Confirm the exact worktree before any edit**

Run:

```powershell
git status --short
git branch --show-current
git rev-parse HEAD
git submodule status
```

Expected: branch `mycar/f4`, HEAD `ed9a2271371c61001fd7440f413b542c2ba64218`, only `?? demo/chassis_mecanum/`, and four `-<sha>` uninitialized gitlinks.

- [ ] **Step 2: Initialize the already-pinned submodules**

Run:

```powershell
git submodule update --init --recursive
git submodule status
```

Expected exact SHAs:

```text
c097c5b581baf9b8cb7cff90bc8a22d2136a2437 pnx_bsp
2349cc108c9ed477ccdcd700e802ea888975cdfd pnx_devices
e7c3e7a2b9d825586ab3e0c413877180c4295df8 pnx_libs
8ba925b60b11fec511a57622c199b57bb23f8f4e pnx_modules
```

Stop if any SHA differs or any submodule is dirty.

- [ ] **Step 3: Re-establish the software baseline**

Run:

```powershell
cmake -S . -B build/host -G Ninja -DPNX_HOST_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
cmake --preset f407-debug
cmake --build --preset f407-debug
```

Expected: host suite and Core build pass. Record exact counts, warnings and exit codes; do not reuse the public worktree's result.

- [ ] **Step 4: Rewrite only the branch-local ownership text**

`AGENTS.md` and `README.md` must state:

```text
This mycar/f4 branch is the vehicle-specific composition.
F407 Board/HAL ownership remains unchanged.
Vehicle code belongs under vehicle/.
Shared pnx_* gitlinks and public APIs remain upstream-owned.
Normal Core/USB/PWM images remain zero-motor images.
Non-zero motor output remains separately authorized.
```

- [ ] **Step 5: Review the boundary diff**

Run:

```powershell
git diff -- AGENTS.md README.md
git status --short
```

Expected: no change under `pnx_*`, `boards/`, or `demo/chassis_mecanum/`.

## Task 2: Add the pure mecanum kinematics

**Files:**

- Create: `vehicle/chassis/types.hpp`
- Create: `vehicle/chassis/kinematics.hpp`
- Create: `vehicle/chassis/kinematics.cpp`
- Create: `tests/host/chassis_kinematics_tests.cpp`
- Modify: `tests/host/CMakeLists.txt`

**Interfaces:**

- Consumes: `body_velocity`, `geometry`.
- Produces: `inverse_kinematics(...) -> std::optional<wheel_vector>`.

- [ ] **Step 1: Add failing host cases**

Test these exact cases with `geometry{0.10F, 0.20F, 0.15F, 100.0F}`:

```cpp
require(equal(inverse_kinematics({1.0F, 0.0F, 0.0F}, g)->rad_s,
              {10.0F, 10.0F, 10.0F, 10.0F}));
require(equal(inverse_kinematics({0.0F, 1.0F, 0.0F}, g)->rad_s,
              {-10.0F, 10.0F, 10.0F, -10.0F}));
require(equal(inverse_kinematics({0.0F, 0.0F, 1.0F}, g)->rad_s,
              {-3.5F, 3.5F, -3.5F, 3.5F}));
require(!inverse_kinematics({}, geometry{}).has_value());
```

Add a saturation case where unconstrained maximum is `200 rad/s`; expected output is uniformly scaled to a maximum magnitude of `100 rad/s`.

- [ ] **Step 2: Run the focused target and observe RED**

Run:

```powershell
cmake --build build/host --target pnx_chassis_kinematics_tests
```

Expected: FAIL because the target or symbols do not exist.

- [ ] **Step 3: Implement the exact X-configuration transform**

```cpp
const float k = g.half_wheelbase_m + g.half_track_m;
wheel_vector out{{
    (cmd.vx_mps - cmd.vy_mps - k * cmd.yaw_rad_s) / g.wheel_radius_m,
    (cmd.vx_mps + cmd.vy_mps + k * cmd.yaw_rad_s) / g.wheel_radius_m,
    (cmd.vx_mps + cmd.vy_mps - k * cmd.yaw_rad_s) / g.wheel_radius_m,
    (cmd.vx_mps - cmd.vy_mps + k * cmd.yaw_rad_s) / g.wheel_radius_m,
}};
```

Reject non-finite inputs, non-positive radius and negative dimensions. If any magnitude exceeds `max_wheel_rad_s`, scale all four by the same positive ratio.

- [ ] **Step 4: Run GREEN**

Run:

```powershell
cmake --build build/host --target pnx_chassis_kinematics_tests
ctest --test-dir build/host -R chassis_kinematics --output-on-failure
```

Expected: focused CTest PASS.

- [ ] **Step 5: Review checkpoint**

Run `git diff --check` and review only the five listed files. Do not commit without separate authorization.

## Task 3: Add DR16 mapping and fail-closed safety state

**Files:**

- Create: `vehicle/chassis/manual_control.hpp`
- Create: `vehicle/chassis/manual_control.cpp`
- Create: `vehicle/chassis/safety_gate.hpp`
- Create: `vehicle/chassis/safety_gate.cpp`
- Create: `tests/host/chassis_manual_safety_tests.cpp`
- Modify: `tests/host/CMakeLists.txt`

**Interfaces:**

- Consumes: normalized stick axes in `[-1, 1]`, remote status, both DR16 switches, motor/CAN/config health.
- Produces: bounded `body_velocity`; latched `safety_state`.

- [ ] **Step 1: Add failing command-mapping tests**

Use `manual_limits{0.05F, 1.0F, 1.0F, 2.0F, 1.0F, -1.0F, -1.0F}` and require:

```cpp
const auto deadbanded = map_manual(
    {true, true, 0.02F, 0.02F, 0.02F}, limits);
require(near(deadbanded.vx_mps, 0.0F));
require(near(deadbanded.vy_mps, 0.0F));
require(near(deadbanded.yaw_rad_s, 0.0F));
require(near(map_manual({true, true, 0.0F, 1.0F, 0.0F}, limits).vx_mps, 1.0F));
require(near(map_manual({true, true, 1.0F, 0.0F, 0.0F}, limits).vy_mps, -1.0F));
require(near(map_manual({true, true, 0.0F, 0.0F, 1.0F}, limits).yaw_rad_s, -2.0F));
const auto offline = map_manual(
    {false, true, 1.0F, 1.0F, 1.0F}, limits);
require(near(offline.vx_mps, 0.0F));
require(near(offline.vy_mps, 0.0F));
require(near(offline.yaw_rad_s, 0.0F));
```

The deadband must be rescaled continuously:

```cpp
sign(x) * (abs(x) - deadband) / (1.0F - deadband)
```

- [ ] **Step 2: Add failing safety-transition tests**

Require this exact behavior:

```text
startup -> disabled
valid config + remote online + switches not both up -> waiting_motors/disabled output
motors online + CAN healthy + rising transition to both switches up -> armed
either switch leaves up -> disabled and PI reset required
remote offline, motor offline or CAN unhealthy while armed -> fault_latched
fault_latched remains zero after health returns
fault clears only after switches leave arm position and reset() is called
```

- [ ] **Step 3: Run RED, implement, then run GREEN**

Run:

```powershell
cmake --build build/host --target pnx_chassis_manual_safety_tests
ctest --test-dir build/host -R chassis_manual_safety --output-on-failure
```

Expected final result: PASS for mapping, arming edge, disarm, offline and latch scenarios.

- [ ] **Step 4: Review checkpoint**

Run `git diff --check`; confirm these files include no ThreadX, HAL, BSP, motor or message headers.

## Task 4: Add deterministic four-wheel velocity PI control

**Files:**

- Create: `vehicle/chassis/velocity_pi.hpp`
- Create: `vehicle/chassis/velocity_pi.cpp`
- Create: `vehicle/chassis/controller.hpp`
- Create: `vehicle/chassis/controller.cpp`
- Create: `tests/host/chassis_controller_tests.cpp`
- Modify: `tests/host/CMakeLists.txt`

**Interfaces:**

- Consumes: `manual_input`, four measured wheel speeds in rad/s, `safety_input`, configuration, and explicit `dt_s`.
- Produces: four signed `std::int16_t` raw current commands plus safety state.

- [ ] **Step 1: Add failing PI tests**

Use a synthetic test configuration `kp=10`, `ki_per_s=20`, `integral_limit_raw=100`, `current_limit_raw=500`. Require:

```text
zero error -> 0
10 rad/s error at dt=0.005 s -> 101 raw on first step
large error -> exactly +500 or -500
continued saturation does not grow the integral beyond 100 raw
reset() -> next zero-error output is 0
dt <= 0 or non-finite input -> 0 and controller reset
```

- [ ] **Step 2: Implement explicit-dt PI with conditional integration**

Use:

```cpp
const float error = target_rad_s - measured_rad_s;
const float candidate_i = std::clamp(
    integral_raw_ + config_.ki_per_s * error * dt_s,
    -config_.integral_limit_raw,
    config_.integral_limit_raw);
const float unsaturated = config_.kp * error + candidate_i;
const float saturated = std::clamp(
    unsaturated, -config_.current_limit_raw, config_.current_limit_raw);
if (unsaturated == saturated || error * unsaturated < 0.0F) {
    integral_raw_ = candidate_i;
}
return static_cast<std::int16_t>(std::lround(saturated));
```

Do not use the shared `control::pid` in v1: its gains are per-update and it has no explicit `dt`; keeping this controller vehicle-local avoids changing a shared contract.

- [ ] **Step 3: Add full-chain controller tests**

Require:

```text
invalid config -> all zero
not armed -> all zero and all PI states reset
armed forward command -> four same-sign wheel targets before motor direction mapping
armed strafe/rotation -> exact FL/FR/RL/RR sign patterns
wheel saturation preserves vector ratios
one safety fault -> all four currents become zero in the same step
direction map {-1,+1,-1,+1} changes motor signs only, not body-frame kinematics
```

- [ ] **Step 4: Run RED/GREEN**

Run:

```powershell
cmake --build build/host --target pnx_chassis_controller_tests
ctest --test-dir build/host -R chassis_controller --output-on-failure
```

Expected: all focused scenarios PASS.

- [ ] **Step 5: Review checkpoint**

Confirm every raw current path is clamped before conversion to `int16_t`, and every disabled/fault path resets all PI state.

## Task 5: Define the fail-closed MyCar configuration and build graph

**Files:**

- Create: `vehicle/chassis/config.hpp`
- Create: `vehicle/chassis/config.cpp`
- Create: `vehicle/mycar.hpp`
- Create: `vehicle/mycar.cpp`
- Create: `configs/vehicles/mycar/params.json`
- Create: `configs/vehicles/mycar/robot.json`
- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`
- Modify: `demo/app.cpp`
- Modify: `tests/host/config_generation_contract.cmake`

**Interfaces:**

- Consumes: existing F407 CAN1, USART3 RX DMA, generated `robot_config.hpp`, `PNX_F407_CAN_ENABLED`, `PNX_F407_USART_ENABLED`.
- Produces: explicit `f407-mycar-chassis-debug` product image; unchanged Core/USB/PWM closures.

- [ ] **Step 1: Describe four M2006s in the generated device tree**

Set `configs/vehicles/mycar/robot.json` exactly to:

```json
{
  "devices": {
    "motors": {
      "list": [
        {"name":"front_left","model":"dji_m2006","can_bus":"can1","can_type":"classic","can_id":"0x201","control_mode":"relax"},
        {"name":"front_right","model":"dji_m2006","can_bus":"can1","can_type":"classic","can_id":"0x202","control_mode":"relax"},
        {"name":"rear_left","model":"dji_m2006","can_bus":"can1","can_type":"classic","can_id":"0x203","control_mode":"relax"},
        {"name":"rear_right","model":"dji_m2006","can_bus":"can1","can_type":"classic","can_id":"0x204","control_mode":"relax"}
      ]
    }
  }
}
```

Create `configs/vehicles/mycar/params.json` from the board defaults, then set
`build.motors.dji=true`, `build.features.motors=true`,
`build.features.remoter=true`, `bindings.remoter_uart="usart3"` and
`remoter.source="dr16"`. Retain 1 Mbps CAN1 and existing IOC ownership.
The files under `configs/boards/dji_c_board_f407/` remain unchanged.

- [ ] **Step 2: Add a safe product configuration**

`config.cpp` must initially return:

```cpp
configuration mycar_configuration() noexcept {
    return {
        .geometry = {0.0F, 0.0F, 0.0F, 0.0F},
        .manual = {0.05F, 0.0F, 0.0F, 0.0F, 1.0F, -1.0F, -1.0F},
        .motor_direction = {1.0F, 1.0F, 1.0F, 1.0F},
        .pi = {0.0F, 0.0F, 0.0F, 0.0F},
        .control_period_s = 0.005F,
    };
}
```

`valid()` returns false until all geometry, speed limits, direction signs, PI limits and `control_period_s` are finite and valid. Zero here is a deliberate fail-closed sentinel, not a guessed car parameter.

- [ ] **Step 3: Add one explicit vehicle product profile**

Add `PNX_ENABLE_MYCAR_CHASSIS` and preset `f407-mycar-chassis-debug`. It must be mutually exclusive with USB/PWM, append only the required sources/includes, and define `PNX_APP_MYCAR_CHASSIS=1`.

Before `generate_config.cmake` is included, select configuration paths exactly
once:

```cmake
if(PNX_ENABLE_MYCAR_CHASSIS)
    set(PNX_BOARD_PARAMS
        "${CMAKE_CURRENT_SOURCE_DIR}/configs/vehicles/mycar/params.json")
    set(PNX_BOARD_ROBOT_CONFIG
        "${CMAKE_CURRENT_SOURCE_DIR}/configs/vehicles/mycar/robot.json")
else()
    set(PNX_BOARD_PARAMS
        "${CMAKE_CURRENT_SOURCE_DIR}/configs/boards/dji_c_board_f407/params.json")
    set(PNX_BOARD_ROBOT_CONFIG
        "${CMAKE_CURRENT_SOURCE_DIR}/configs/boards/dji_c_board_f407/robot.json")
endif()
```

This prevents DR16 and motor feature flags from leaking into Core, USB or PWM
generated configuration.

Required closure includes:

```text
boards/dji_c_board_f407/bsp/bsp_can.cpp
boards/dji_c_board_f407/bsp/bsp_usart.cpp
pnx_libs/msg/src/msg.cpp
pnx_modules/remoter/src/dr16.cpp
pnx_modules/remoter/src/remoter.cpp
pnx_devices/motors/motor/src/motorhandler.cpp
pnx_devices/motors/dji/src/djimotors.cpp
pnx_devices/motors/dji/src/djimotorhandler.cpp
vehicle/mycar.cpp
vehicle/chassis/*.cpp (explicit list; no recursive glob)
```

Core, USB and PWM source lists remain unchanged.

- [ ] **Step 4: Route only the car profile through `demo/app.cpp`**

Extend the exactly-one compile-time assertion and call:

```cpp
#elif defined(PNX_APP_MYCAR_CHASSIS)
    vehicle::mycar::run();
#endif
```

- [ ] **Step 5: Test generated configuration and closure isolation**

Run:

```powershell
cmake -S . -B build/host -G Ninja -DPNX_HOST_TESTS=ON
cmake --build build/host
ctest --test-dir build/host --output-on-failure
cmake --preset f407-debug
cmake --build --preset f407-debug
cmake --preset f407-mycar-chassis-debug
cmake --build --preset f407-mycar-chassis-debug
```

Expected:

- generated config reports DR16 on USART3 and four M2006 configurations;
- Core has CAN/USART disabled and no vehicle symbols;
- car image has CAN/USART enabled and exactly one `app_start`;
- car image builds while configuration remains invalid and zero-output.

## Task 6: Integrate ThreadX, DR16 and four M2006s

**Files:**

- Create: `vehicle/chassis/runtime.hpp`
- Create: `vehicle/chassis/runtime.cpp`
- Modify: `vehicle/mycar.cpp`
- Create: `tests/host/chassis_runtime_policy_tests.cpp`
- Modify: `tests/host/CMakeLists.txt`

**Interfaces:**

- Consumes: `remoter::state`, `motors::m2006`, `motors::djimotorhandler`, `bsp::can::snapshot(can1)`, pure `controller`.
- Produces: 200 Hz bounded current commands and zero-current safety behavior.

- [ ] **Step 1: Add runtime-policy tests without ThreadX**

Extract a pure `runtime_policy` and require:

```text
remoter.offline or active_source != dr16 -> remote_online=false
both switches must be up to request arm
CAN state other than active -> can_healthy=false
change in CAN error_count, drop_count or fault_epoch -> fault
motor watchdog not yet sampled -> all_motors_online=false
any failed registration -> startup failure and all zero
```

- [ ] **Step 2: Instantiate the generated four-M2006 map**

Use:

```cpp
motors::m2006 front_left{robot::motors::front_left};
motors::m2006 front_right{robot::motors::front_right};
motors::m2006 rear_left{robot::motors::rear_left};
motors::m2006 rear_right{robot::motors::rear_right};
```

Register all four before creating the control thread. On any failure, relax all motors, send one zero frame if CAN is healthy, record startup fault and return.

- [ ] **Step 3: Start remoter and subscribe before control**

Call `remoter::service::instance().init(cfg)`, then `msg::subscribe<remoter::state>()`. Potentially suspending calls belong inside the runtime thread, not directly inside `app_start()`.

- [ ] **Step 4: Run the exact 200 Hz sequence**

Each 5-tick iteration:

```text
read latest remoter state
snapshot CAN telemetry
every 4 iterations call handler.alive_check() (20 ms watchdog)
form safety_input
run pure controller with dt=0.005 s
apply configured motor_direction signs
clamp each command to configured current_limit_raw
set_current() only while armed; otherwise relax()
handler.send_control()
sleep until the next absolute 5-tick deadline
```

Use an absolute next-wakeup tick to avoid cumulative drift. If the loop overruns, relax all motors, send zero, latch fault and resynchronize the deadline.

- [ ] **Step 5: Add observability**

Expose one retained/debug structure containing:

```cpp
struct telemetry {
    safety_state state;
    std::uint32_t loop_count;
    std::uint32_t overrun_count;
    std::uint32_t remote_update_count;
    std::array<float, 4> target_rad_s;
    std::array<float, 4> measured_rad_s;
    std::array<std::int16_t, 4> current_raw;
    bsp::can::telemetry can;
};
```

Do not add USB, a runtime registry or a new shared telemetry framework.

- [ ] **Step 6: Re-run the complete software gate**

Run all host tests, Core build and car build. Expected: PASS, zero compiler/linker warnings, and `git diff --check` PASS.

## Task 7: Populate measured car parameters

**Files:**

- Modify: `vehicle/chassis/config.cpp`
- Record: `.codex/tasks/2026-07-31-f4-mycar-dr16-m2006-chassis/evidence-map.md`
- Record: `.codex/tasks/2026-07-31-f4-mycar-dr16-m2006-chassis/experiment-record.md`

**Interfaces:**

- Consumes: physical measurements and verified wheel/CAN mapping.
- Produces: measured geometry/direction fields in a still-invalid,
  zero-current configuration.

- [ ] **Step 1: Perform a power-off inspection**

Record:

- wheel radius in metres at the effective contact surface;
- front/rear axle centre distance, then `half_wheelbase_m = distance / 2`;
- left/right wheel centre distance, then `half_track_m = distance / 2`;
- FL/FR/RL/RR physical labels;
- controller IDs producing feedback `0x201` through `0x204`;
- motor-positive direction for each wheel.

No number may be copied from the discarded prototype.

- [ ] **Step 2: Enter geometry and direction only**

Set measured geometry, direction signs (`+1` or `-1`) and conservative manual maxima. Keep PI gains and `current_limit_raw` at zero.

- [ ] **Step 3: Verify fail-closed build**

The image must build but `valid()` must remain false because non-zero actuation has not been authorized. Host tests use synthetic non-zero configs and remain PASS.

## Task 8: Staged attended hardware acceptance

**Files:**

- Modify after authorization: `vehicle/chassis/config.cpp`
- Update after observations: task packet `experiment-record.md` and `validation-report.md`
- Update at task close: `HANDOFF.md`, `VAULT_UPDATE.md`

**Interfaces:**

- Consumes: explicit flash/hardware/non-zero-output authorization.
- Produces: separately labelled flash, CAN/feedback, wheel-direction, closed-loop and ground-test evidence.

- [ ] **Gate H0: Obtain authorization**

Before flashing or energizing, obtain explicit permission for the named board, CAN bus, four M2006s, current limit and test procedure.

- [ ] **Stage H1: Zero-current image**

With wheels lifted and emergency power removal available:

1. flash and verify the car ELF;
2. keep `current_limit_raw=0`;
3. confirm DR16 `update_count` advances;
4. confirm all four feedback IDs arrive and motor watchdog reports online;
5. confirm switch/offline/CAN faults keep all four current commands at zero.

Stop on any mapping mismatch, CAN error/drop/fault increment, unexpected motion or missing feedback.

- [ ] **Stage H2: One wheel at a time**

After separate non-zero authorization, use `current_limit_raw <= 500` for the first test and a `1 rad/s` wheel target. Test FL, FR, RL, RR individually; verify observed wheel identity and forward-positive direction. Immediately disarm after each observation.

- [ ] **Stage H3: Four-wheel lifted closed loop**

Use low-speed forward, strafe and rotation commands. Pass criteria:

- expected wheel sign pattern in all three modes;
- no CAN error/drop/fault increase;
- each wheel steady-state speed error within 10% after 1 second;
- no sustained current saturation, oscillation, unexpected heating or mechanical interference;
- switch disarm reaches zero command within 20 ms;
- DR16 loss reaches zero command within 150 ms.

- [ ] **Stage H4: Low-speed ground test**

Only after H3 passes, begin at `max_vx <= 0.20 m/s`, `max_vy <= 0.20 m/s`, `max_yaw <= 0.50 rad/s`. Test forward, reverse, left/right strafe, clockwise/counter-clockwise rotation and release-to-zero. Run three repetitions, then a 120-second low-speed soak while recording CAN health, target/measured speeds, currents and safety state.

- [ ] **Stage H5: Close the evidence**

Keep static, Host, build, flash, lifted-wheel, ground-motion and repeated-soak results in separate rows. Do not call the first version complete if physical motion or repeatability was not observed.

## Final validation matrix

| Layer | Required result |
|---|---|
| Static | No HAL/ThreadX includes in pure chassis files; no shared submodule changes |
| Host | Existing suite plus kinematics, manual/safety, PI/controller and runtime-policy tests PASS |
| Core regression | `f407-debug` builds with no CAN/USART/vehicle closure |
| Car build | `f407-mycar-chassis-debug` builds warning-free with exactly one `app_start` |
| Zero-output bench | DR16 and four feedback IDs observed; every unsafe state commands zero |
| Lifted-wheel | Four wheel identities/directions verified independently |
| Closed loop | <=10% steady-state speed error at low target; no sustained saturation |
| Safety | switch disarm <=20 ms; DR16 loss <=150 ms; CAN/motor fault latches zero |
| Ground | six low-speed directions correct, three repetitions and 120 s soak |

## Assumptions and explicit open gates

- User-approved facts: work will be done in `pnx_f4_mycar`; v1 uses DR16 manual closed loop and four M2006s.
- Repository facts: car worktree is `mycar/f4@ed9a227`; submodules are currently uninitialized; USART3 RX DMA, CAN1 at 1 Mbps and ThreadX 1000 ticks/s exist in the IOC.
- Deliberately unconfigured until measured: wheel radius, wheelbase, track width, physical motor directions and confirmed wheel-to-ID mapping.
- Deliberately zero until hardware approval/tuning: chassis speed limits, PI gains, integral/current limits.
- Shared-interface changes are out of scope. If `djimotorhandler::send_control()` returning `void` prevents adequate fault evidence, stop and open a separate shared-contract decision rather than modifying the submodule inside this task.
- Commit and push remain unauthorized.
