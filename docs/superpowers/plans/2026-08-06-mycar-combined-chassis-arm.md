# MyCar Combined Chassis and ARM Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and flash one F407 image that safely routes one DR16 between the validated four-M2006 mecanum chassis and the complete J1-J4 arm.

**Architecture:** Add a vehicle-specific combined product whose one runtime owns DR16 ingest, all five DJI motor registrations, the aggregate watchdog, PWM, the 200 Hz deadline, and one CAN send per cycle. Reuse the existing chassis controller/policy and ARM control/policy classes without starting either standalone hardware runtime.

**Tech Stack:** C++17, CMake 3.22 presets, ThreadX, PnX DR16/message service, DJI M2006/C610 CAN driver, F407 TIM1 PWM, native Host CTest, GNU Arm Embedded, OpenOCD CMSIS-DAP.

## Global Constraints

- Keep `f407-mycar-chassis-debug` and `f407-arm-debug` behavior unchanged.
- Do not modify `pnx_bsp`, `pnx_devices`, `pnx_libs`, or `pnx_modules`.
- Keep FL/FR/RL/RR/J1 CAN feedback IDs at `0x201/0x202/0x204/0x203/0x205`.
- Keep ARM PWM and tuning values from `vehicle/arm/runtime/arm_config.cpp` unchanged.
- Use exactly one remoter initialization, one remoter subscription, one global `alive_check()`, and one normal-cycle `send_control()`.
- Mask chassis output outside `left_sw == up`; mask ARM manual motion outside `left_sw == low`; preserve healthy ARM hold while `right_sw == up`.
- Any stale remote, missing required feedback, CAN fault, PWM failure, invalid configuration, or deadline overrun must remove unsafe output.
- The user authorized implementation, build, and flashing. Do not push. Do not create implementation commits without separate authorization.

## File Structure

- `vehicle/combined/control/mode_router.*`: pure stateful chassis-entry interlock and mode mapping.
- `vehicle/combined/runtime/runtime.*`: sole combined hardware owner and telemetry surface.
- `vehicle/combined.hpp`, `vehicle/combined.cpp`: product entry point.
- `configs/vehicles/mycar_combined/*`: union product generation inputs.
- `tests/host/combined_mode_router_tests.cpp`: executable policy tests.
- `tests/host/combined_product_contract.cmake`: selector, preset, and five-motor JSON contract.
- `tests/host/combined_runtime_source_contract.cmake`: static ownership and output-path contract.
- `CMakeLists.txt`, `CMakePresets.json`, `demo/app.cpp`, `tests/host/CMakeLists.txt`: product and test closure wiring.
- `HANDOFF.md`, `VAULT_UPDATE.md`: exact implementation/build/flash evidence and remaining attended checks.

---

### Task 1: Mode Router and Chassis Entry Interlock

**Files:**
- Create: `vehicle/combined/control/mode_router.hpp`
- Create: `vehicle/combined/control/mode_router.cpp`
- Create: `tests/host/combined_mode_router_tests.cpp`
- Modify: `tests/host/CMakeLists.txt`

**Interfaces:**
- Consumes: `remoter::state`, `remoter::sw_state`, configured axis deadbands.
- Produces: `vehicle::combined::control_mode`, `mode_router_output`, and `mode_router::update(const remoter::state&, float, float)`.

- [ ] **Step 1: Add the failing mode-router test target**

Cover these exact scenarios in `combined_mode_router_tests.cpp`:

```cpp
using vehicle::combined::control_mode;
using vehicle::combined::mode_router;

mode_router router{};
auto remote = online_centered_dr16();

remote.left_sw = remoter::sw_state::up;
remote.right_sw = remoter::sw_state::up;
auto output = router.update(remote, 0.05F, 0.05F);
require(output.mode == control_mode::chassis);
require(output.chassis_ready);
require(output.chassis_remote.right_sw == remoter::sw_state::up);

remote.left_x = 0.5F;
remote.left_sw = remoter::sw_state::low;
output = router.update(remote, 0.05F, 0.05F);
require(output.mode == control_mode::arm);
require(!output.chassis_ready);
require(output.chassis_remote.right_sw == remoter::sw_state::mid);

remote.left_sw = remoter::sw_state::up;
output = router.update(remote, 0.05F, 0.05F);
require(!output.chassis_ready);
remote.left_x = 0.0F;
output = router.update(remote, 0.05F, 0.05F);
require(output.chassis_ready);

remote.offline = true;
output = router.update(remote, 0.05F, 0.05F);
require(output.mode == control_mode::neutral);
require(!output.chassis_ready);
```

Register `pnx_combined_mode_router_tests` as CTest name
`combined_mode_router` and include `tests/host/fakes`, repository root,
`pnx_modules/remoter/include`, and `pnx_libs/common/include`.

- [ ] **Step 2: Run the focused test and verify RED**

Run:

```powershell
cmake -S . -B build/host-combined -DPNX_HOST_TESTS=ON
cmake --build build/host-combined --target pnx_combined_mode_router_tests
```

Expected: configure or compile fails because the combined router files/types do not exist.

- [ ] **Step 3: Implement the pure router**

Define:

```cpp
enum class control_mode : std::uint8_t { neutral, chassis, arm };

struct mode_router_output {
    control_mode mode{control_mode::neutral};
    bool remote_online{};
    bool chassis_ready{};
    bool arm_axes_centered{};
    remoter::state chassis_remote{};
};

class mode_router {
public:
    mode_router_output update(const remoter::state& remote,
                              float chassis_deadband,
                              float arm_deadband) noexcept;
    void reset() noexcept;
private:
    bool previous_chassis_selected_{};
    bool chassis_ready_{};
};
```

`update()` must validate finite axes/deadbands, map left switch
`up/mid/low` to chassis/neutral/ARM, and keep `chassis_remote.right_sw` at
`mid` with zeroed chassis axes until a chassis entry or re-entry has observed
centered `left_x`, `left_y`, and `right_x`. Once ready, retain readiness while
the mode and remote remain valid; clear it on mode exit, offline input, or
right-switch release. A held-up right switch may become ready when the axes
return to center, preventing a mode-transition jump without dropping ARM hold.

- [ ] **Step 4: Run the focused test and verify GREEN**

```powershell
cmake --build build/host-combined --target pnx_combined_mode_router_tests
ctest --test-dir build/host-combined -R "^combined_mode_router$" --output-on-failure
```

Expected: `1/1` PASS.

- [ ] **Step 5: Record a review checkpoint**

Run `git diff --check` and inspect only the router/test/CMake hunks. Do not commit under the current authorization.

### Task 2: Combined Product Selector and Generated Configuration

**Files:**
- Create: `configs/vehicles/mycar_combined/params.json`
- Create: `configs/vehicles/mycar_combined/robot.json`
- Create: `vehicle/combined.hpp`
- Create: `vehicle/combined.cpp`
- Create: `tests/host/combined_product_contract.cmake`
- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`
- Modify: `demo/app.cpp`
- Modify: `tests/host/CMakeLists.txt`

**Interfaces:**
- Consumes: `vehicle::chassis::mycar_configuration()`, `vehicle::arm::arm_configuration()`.
- Produces: `vehicle::combined::run() noexcept`, `PNX_ENABLE_MYCAR_COMBINED`, `PNX_APP_MYCAR_COMBINED`, preset `f407-mycar-combined-debug`.

- [ ] **Step 1: Add the failing product contract**

The CMake script must parse the combined `robot.json` and require exactly:

```text
front_left  0x201
front_right 0x202
rear_left   0x204
rear_right  0x203
j1          0x205
```

It must also read `CMakeLists.txt`, `CMakePresets.json`, and `demo/app.cpp`
and require all combined selector, preset, and app-entry tokens. Register it
as CTest name `combined_product_contract`.

- [ ] **Step 2: Run the contract and verify RED**

```powershell
cmake -S . -B build/host-combined -DPNX_HOST_TESTS=ON
ctest --test-dir build/host-combined -R "^combined_product_contract$" --output-on-failure
```

Expected: FAIL because the combined selector/configuration files are absent.

- [ ] **Step 3: Add the product closure**

Create the union `robot.json` with the exact five entries above. Copy the
identical existing DR16/build feature parameters into combined `params.json`.
Add `PNX_ENABLE_MYCAR_COMBINED` to every mutually exclusive selector list and
set it explicitly false in every old preset. Add a debug preset that sets only
the combined selector true.

Implement the entry point as:

```cpp
namespace vehicle::combined {
void run() noexcept
{
    runtime::start(chassis::mycar_configuration(),
                   arm::arm_configuration());
}
}
```

Wire `PNX_APP_MYCAR_COMBINED` into `demo/app.cpp`. The combined CMake source
closure includes CAN, USART, PWM, message/remoter, DJI motor sources, both
subsystems' pure control/config/policy sources, and only
`vehicle/combined/runtime/runtime.cpp` as its hardware runtime.

- [ ] **Step 4: Run the product contract and verify GREEN**

```powershell
cmake -S . -B build/host-combined -DPNX_HOST_TESTS=ON
ctest --test-dir build/host-combined -R "^combined_product_contract$" --output-on-failure
```

Expected: `1/1` PASS.

- [ ] **Step 5: Record a review checkpoint**

Run `git diff --check` and verify no old preset can inherit a stale true combined selector. Do not commit.

### Task 3: Single-Owner Combined Hardware Runtime

**Files:**
- Create: `vehicle/combined/runtime/runtime.hpp`
- Create: `vehicle/combined/runtime/runtime.cpp`
- Create: `tests/host/combined_runtime_source_contract.cmake`
- Modify: `tests/host/CMakeLists.txt`

**Interfaces:**
- Consumes: existing chassis/ARM configurations, controllers, policies, J1 control primitives, servo map/output, remoter service, DJI motor handler.
- Produces: `runtime::start(const chassis::configuration&, const arm::configuration&) noexcept` and `runtime::debug_state() noexcept`.

- [ ] **Step 1: Add the failing source-ownership contract**

Require the combined runtime source to contain exactly one each of
`msg::read(` and `motor_handler.alive_check()`, exactly five
`register_motor(` calls, and exactly one `motor_handler.send_control()` inside
the normal control loop. A separate bounded startup-zero call is permitted
outside the loop.
Require `remote_ingest_entry`, `control_entry`, all four chassis motor symbols,
`j1_motor`, `j1_hold.update`, all three PWM updates, and both subsystem policy
updates. Reject `vehicle::mycar::run(` and `vehicle::arm::run(`. Register the
script as CTest name `combined_runtime_source_contract`.

- [ ] **Step 2: Run the contract and verify RED**

```powershell
cmake -S . -B build/host-combined -DPNX_HOST_TESTS=ON
ctest --test-dir build/host-combined -R "^combined_runtime_source_contract$" --output-on-failure
```

Expected: FAIL because the combined runtime is absent.

- [ ] **Step 3: Define telemetry and startup API**

Use nested existing telemetry so the debugger can inspect both products:

```cpp
enum class runtime_fault : std::uint32_t {
    none = 0U,
    invalid_config = 1U << 0U,
    registration_failed = 1U << 1U,
    remoter_init_failed = 1U << 2U,
    subscribe_failed = 1U << 3U,
    thread_create_failed = 1U << 4U,
    pwm_failed = 1U << 5U,
    overrun = 1U << 6U,
    subsystem_fault = 1U << 7U,
};

struct telemetry {
    control_mode mode{control_mode::neutral};
    runtime_fault faults{runtime_fault::none};
    bool watchdog_sampled{};
    bool all_motors_online{};
    std::uint32_t loop_count{};
    std::uint32_t overrun_count{};
    std::uint32_t remote_update_count{};
    chassis::telemetry chassis{};
    arm::telemetry arm{};
    bsp::can::telemetry can{};
};
```

- [ ] **Step 4: Implement one startup path**

Instantiate generated motors in FL/FR/RL/RR/J1 order, one remoter subscriber,
one ingest thread, one 2048-byte-or-larger control stack, and J2/J3/J4 PWM
objects. `start()` initializes both subsystem configurations, registers all
five motors, snapshots the CAN baseline after registration, constructs both
policies, sends at most one startup zero frame, and creates one control thread.
Any partial startup failure relaxes every registered motor and stops PWM.

- [ ] **Step 5: Implement the 5 ms control cycle**

The cycle order is fixed:

```cpp
const auto remote_snapshot = copy_remote_snapshot();
const auto routed = router.update(control_remote,
                                  chassis_config.manual.deadband,
                                  arm_config.servos.deadband);
const auto can = bsp::can::snapshot(bsp::can::bus::can1);

// One interrupt-protected feedback snapshot; one aggregate check per 20 ms.
if (watchdog_phase.advance()) {
    all_motors_online = motor_handler.alive_check();
    watchdog_sampled = true;
}

const auto chassis_policy_output = chassis_policy.update(chassis_input);
const auto arm_policy_output = arm_policy.update(arm_input);
const auto chassis_output = chassis_controller.update(
    chassis_policy_output.manual, measured_wheels,
    chassis::controller_safety_for(chassis_policy_output), control_period_s);
const auto arm_state = arm_safety.update(
    arm::controller_safety_for(arm_policy_output));

const bool common_healthy = watchdog_sampled && all_motors_online &&
    chassis_policy_output.safety.remote_online &&
    chassis_policy_output.safety.can_healthy &&
    arm_policy_output.safety.can_healthy;
const bool terminal_fault = chassis_policy.fault_latched() ||
    arm_policy.fault_latched() || combined_fault_latched();
const bool chassis_enabled = routed.mode == control_mode::chassis &&
    routed.chassis_ready && common_healthy && !terminal_fault &&
    chassis::should_set_current(chassis_policy_output,
                                chassis_output.state);
const bool arm_manual_enabled = common_healthy && !terminal_fault &&
    arm::should_enable_outputs(arm_policy_output, arm_state);

// Feed arm_manual_enabled through the existing J1 zero/stall/manual/PID/
// gravity chain. Feed its result and raw right-switch health through
// j1_hold.update(); feed the three servo axes through servo_control.update().
// Select four chassis currents only when chassis_enabled. Otherwise relax
// those four motors. Select J1 current or a healthy hold current only through
// j1_hold. Stop all PWM and relax all five motors when common_healthy is false
// or terminal_fault is true.
motor_handler.send_control();
publish_telemetry(next_telemetry);
```

Use raw `right_sw` for ARM hold health, but use the router's sanitized remote
for chassis arming. If either subsystem policy has a terminal fault, or PWM
update fails, latch `subsystem_fault`/`pwm_failed` and zero both products.

- [ ] **Step 6: Run all three focused combined tests**

```powershell
cmake --build build/host-combined
ctest --test-dir build/host-combined -R "^combined_" --output-on-failure
```

Expected: `3/3` PASS.

- [ ] **Step 7: Record a review checkpoint**

Inspect source ownership, output selection, overrun paths, and `git diff --check`. Do not commit.

### Task 4: Full Validation, Artifact, Flash, and Handoff

**Files:**
- Modify: `HANDOFF.md`
- Modify: `VAULT_UPDATE.md`

**Interfaces:**
- Consumes: completed combined source and preset.
- Produces: verified ELF, programmed C-board, exact evidence record.

- [ ] **Step 1: Run the complete Host suite once**

```powershell
cmake --build build/host-combined
ctest --test-dir build/host-combined --output-on-failure
```

Expected: all existing 53 tests plus three combined tests pass (`56/56`).

- [ ] **Step 2: Configure and build the combined image**

```powershell
cmake --fresh --preset f407-mycar-combined-debug
cmake --build --preset f407-mycar-combined-debug
```

Expected: configure, compile, and link PASS with
`build/f407-mycar-combined-debug/pnx_embedded.elf` present.

- [ ] **Step 3: Inspect closure and hash the artifact**

Use `arm-none-eabi-nm` to require combined runtime symbols and reject both
standalone runtime start symbols. Record `Get-FileHash -Algorithm SHA256`, ELF
size, RAM/Flash link output, branch, HEAD, and working-tree state.

- [ ] **Step 4: Flash with the authorized safe setup**

Preconditions already supplied by the user: one C-board carries all devices;
before programming verify `right_sw == mid` and chassis is raised. Run:

```powershell
& "D:\OpenOCD\bin\openocd.exe" `
  -s "D:\OpenOCD\share\openocd\scripts" `
  -f "interface/cmsis-dap.cfg" `
  -f "target/stm32f4x.cfg" `
  -c "adapter speed 2000" `
  -c "program build/f407-mycar-combined-debug/pnx_embedded.elf verify reset exit"
```

Expected: `Programming Finished`, `Verified OK`, and target reset.

- [ ] **Step 5: Perform the non-moving post-flash gate**

Observe no motion with `left_sw == mid` and `right_sw == mid`. Do not perform
unattended non-zero movement. Report the exact next attended sequence:
chassis directions, ARM axes one at a time, mode transitions, DR16 loss, and
loaded hold.

- [ ] **Step 6: Update evidence documents**

Record only observed test/build/flash facts in `HANDOFF.md`. Put the proposed
durable Vault synthesis in `VAULT_UPDATE.md`; do not modify the Vault. Run
`git diff --check` and summarize the uncommitted implementation state.
