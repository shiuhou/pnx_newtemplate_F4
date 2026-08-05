#include "vehicle/arm/runtime/arm_runtime.hpp"

// 可執行規格：arm-clean runtime policy 只恢復 J1；只信任 fresh DR16，且 J1 / CAN
// 任一失效都 force zero。J3/夾爪 runtime 仍留在後續 adapter / remoter API 決策。

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace
{

using namespace vehicle::arm;

static_assert(std::is_same_v<decltype(runtime::debug_state()), telemetry>);

void require(bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}

constexpr bool has(runtime_fault value, runtime_fault expected) noexcept
{
    return (static_cast<std::uint32_t>(value) &
            static_cast<std::uint32_t>(expected)) != 0U;
}

bsp::can::telemetry active_can() noexcept
{
    bsp::can::telemetry value{};
    value.rx_count = 10U;
    value.tx_count = 11U;
    value.last_id = 0x201U;
    value.last_tick = 12U;
    value.error_count = 13U;
    value.bus_off_count = 14U;
    value.drop_count = 15U;
    value.fault_epoch = 16U;
    value.bus_state = bsp::can::state::active;
    return value;
}

remoter::state online_dr16() noexcept
{
    remoter::state value{};
    value.offline = false;
    value.active_source = remoter::source::dr16;
    value.left_sw = remoter::sw_state::low;
    value.right_sw = remoter::sw_state::up;
    value.right_x = 0.25F;
    value.right_y = 0.75F;
    value.left_x = -0.75F;
    value.left_y = -0.50F;
    return value;
}

runtime_policy_input healthy_input(
    const bsp::can::telemetry& can_baseline) noexcept
{
    runtime_policy_input value{};
    value.remote = online_dr16();
    value.can = can_baseline;
    value.watchdog_sampled = true;
    value.j1_online = true;
    value.manual_axes_centered = true;
    return value;
}

void require_healthy_output(const runtime_policy_output& output) noexcept
{
    require(output.manual.online);
    require(output.manual.mode == control_mode::arm);
    require(output.manual.arm_mode_selected);
    require(output.manual.right_switch_up);
    require(output.manual.enable_switches_up);
    require(output.manual.j1_axis == 0.75F);
    require(output.manual.j2_axis == -0.50F);
    require(output.manual.j3_axis == 0.25F);
    require(output.manual.gripper_axis == -0.75F);
    require(output.safety.remote_online);
    require(output.safety.enable_switches_up);
    require(output.safety.j1_online);
    require(output.safety.can_healthy);
    require(output.safety.config_valid);
    require(!output.force_zero);
    require(!output.fault_latched);
}

void arm_gate(arm_safety_gate& gate) noexcept
{
    require(gate.update({true, true, false, true, true, true, true}) ==
            arm_safety_state::disabled);
    require(!gate.output_enabled());
    require(gate.update({true, true, true, true, true, true, true}) ==
            arm_safety_state::armed);
    require(gate.output_enabled());
}

void test_safety_requires_release_then_rise() noexcept
{
    arm_safety_gate gate;
    require(gate.update({true, true, true, true, true, true, true}) ==
            arm_safety_state::disabled);
    require(gate.update({true, true, true, true, true, true, true}) ==
            arm_safety_state::disabled);
    arm_gate(gate);
}

void test_safety_fault_latch_and_reset() noexcept
{
    arm_safety_gate gate;
    arm_gate(gate);
    require(gate.update({false, true, true, true, true, true, true}) ==
            arm_safety_state::fault_latched);
    require(!gate.output_enabled());
    gate.reset();
    require(gate.update({true, true, true, true, true, true, true}) ==
            arm_safety_state::fault_latched);
    require(gate.update({true, true, false, true, true, true, true}) ==
            arm_safety_state::fault_latched);
    gate.reset();
    require(gate.update({true, true, false, true, true, true, true}) ==
            arm_safety_state::disabled);
}

void test_arm_mode_requires_initial_release_and_allows_centered_resume() noexcept
{
    arm_safety_gate gate;

    require(gate.update({true, false, true, true, true, true, true}) ==
            arm_safety_state::disabled);
    require(gate.update({true, true, true, true, true, true, true}) ==
            arm_safety_state::disabled);

    require(gate.update({true, true, false, false, true, true, true}) ==
            arm_safety_state::disabled);
    require(gate.update({true, true, true, true, true, true, true}) ==
            arm_safety_state::disabled);

    arm_gate(gate);
    require(gate.update({true, false, true, true, true, true, true}) ==
            arm_safety_state::disabled);
    require(!gate.output_enabled());
    require(gate.update({true, true, true, true, true, true, true}) ==
            arm_safety_state::armed);
    require(gate.output_enabled());
}

void test_arm_mode_resume_is_cleared_by_disarm_or_health_loss() noexcept
{
    arm_safety_gate disarmed{};
    arm_gate(disarmed);
    require(disarmed.update({true, false, false, true, true, true, true}) ==
            arm_safety_state::disabled);
    require(disarmed.update({true, true, true, true, true, true, true}) ==
            arm_safety_state::disabled);

    arm_safety_gate disconnected{};
    arm_gate(disconnected);
    require(disconnected.update({true, false, true, true, true, true, true}) ==
            arm_safety_state::disabled);
    require(disconnected.update({false, false, true, true, true, true, true}) ==
            arm_safety_state::waiting_remote);
    require(disconnected.update({true, true, true, true, true, true, true}) ==
            arm_safety_state::disabled);
}

void test_remote_source_switches_and_axis() noexcept
{
    const auto baseline = active_can();

    runtime_policy offline_policy{true, true, baseline};
    auto input = healthy_input(baseline);
    input.remote.offline = true;
    auto output = offline_policy.update(input);
    require(!output.manual.online);
    require(!output.safety.remote_online);
    require(output.force_zero);

    for (const remoter::source wrong_source : {
             remoter::source::none,
             remoter::source::vt03,
             remoter::source::ps2,
         })
    {
        runtime_policy wrong_source_policy{true, true, baseline};
        input = healthy_input(baseline);
        input.remote.active_source = wrong_source;
        output = wrong_source_policy.update(input);
        require(!output.manual.online);
        require(!output.safety.remote_online);
        require(output.force_zero);
    }

    constexpr std::array<remoter::sw_state, 3U> switch_positions{
        remoter::sw_state::low,
        remoter::sw_state::mid,
        remoter::sw_state::up,
    };
    for (const auto left : switch_positions)
    {
        for (const auto right : switch_positions)
        {
            runtime_policy switch_policy{true, true, baseline};
            input = healthy_input(baseline);
            input.remote.left_sw = left;
            input.remote.right_sw = right;
            output = switch_policy.update(input);
            const bool arm_selected = left == remoter::sw_state::low;
            const bool right_up = right == remoter::sw_state::up;
            require(output.manual.arm_mode_selected == arm_selected);
            require(output.manual.right_switch_up == right_up);
            require(output.manual.enable_switches_up ==
                    (arm_selected && right_up));
            require(output.safety.arm_mode_selected == arm_selected);
            require(output.safety.enable_switches_up == right_up);
            require(output.force_zero == !arm_selected);
        }
    }

    runtime_policy axis_policy{true, true, baseline};
    require_healthy_output(axis_policy.update(healthy_input(baseline)));
}

void test_j1_hold_requires_prior_arm_and_healthy_right_switch() noexcept
{
    j1_hold_gate gate{};
    j1_hold_input input{
        false,
        true,
        true,
        true,
        true,
        true,
        false,
    };

    require(!gate.update(input));
    input.arm_control_enabled = true;
    require(gate.update(input));

    input.arm_control_enabled = false;
    require(gate.update(input));

    input.right_switch_up = false;
    require(!gate.update(input));
    input.right_switch_up = true;
    require(!gate.update(input));

    input.arm_control_enabled = true;
    require(gate.update(input));
    input.arm_control_enabled = false;
    input.can_healthy = false;
    require(!gate.update(input));

    input.can_healthy = true;
    input.arm_control_enabled = true;
    require(gate.update(input));
    gate.reset();
    input.arm_control_enabled = false;
    require(!gate.update(input));
}

void test_j2_hold_requires_existing_pwm_and_healthy_right_switch() noexcept
{
    const auto baseline = active_can();
    runtime_policy policy{true, true, baseline};
    auto output = policy.update(healthy_input(baseline));

    output.manual.mode = control_mode::chassis;
    output.manual.arm_mode_selected = false;
    output.manual.enable_switches_up = false;
    output.safety.arm_mode_selected = false;
    output.safety.enable_switches_up = false;
    output.force_zero = true;

    require(!should_hold_j2_output(false, output));
    require(should_hold_j2_output(true, output));

    output.manual.right_switch_up = false;
    require(!should_hold_j2_output(true, output));
    output.manual.right_switch_up = true;

    output.manual.online = false;
    require(!should_hold_j2_output(true, output));
    output.manual.online = true;

    output.safety.j1_online = false;
    require(!should_hold_j2_output(true, output));
    output.safety.j1_online = true;

    output.safety.can_healthy = false;
    require(!should_hold_j2_output(true, output));
    output.safety.can_healthy = true;

    output.safety.config_valid = false;
    require(!should_hold_j2_output(true, output));
    output.safety.config_valid = true;

    output.fault_latched = true;
    require(!should_hold_j2_output(true, output));
}

void test_watchdog_and_can_health() noexcept
{
    const auto baseline = active_can();

    runtime_policy unsampled{true, true, baseline};
    auto input = healthy_input(baseline);
    input.watchdog_sampled = false;
    auto output = unsampled.update(input);
    require(!output.safety.j1_online);
    require(output.force_zero);
    require(!output.fault_latched);

    runtime_policy j1_fault{true, true, baseline};
    input = healthy_input(baseline);
    input.j1_online = false;
    output = j1_fault.update(input);
    require(!output.safety.j1_online);
    require(output.force_zero);

    for (const bsp::can::state state : {
             bsp::can::state::warning,
             bsp::can::state::passive,
         })
    {
        runtime_policy can_policy{true, true, baseline};
        input = healthy_input(baseline);
        input.can.bus_state = state;
        output = can_policy.update(input);
        require(output.safety.can_healthy);
        require(!output.force_zero);
        require(!output.fault_latched);
    }

    for (const bsp::can::state state : {
             bsp::can::state::stopped,
             bsp::can::state::bus_off,
             bsp::can::state::recovering,
             bsp::can::state::fault,
         })
    {
        runtime_policy can_policy{true, true, baseline};
        input = healthy_input(baseline);
        input.can.bus_state = state;
        output = can_policy.update(input);
        require(!output.safety.can_healthy);
        require(output.force_zero);
        require(!output.fault_latched);
    }

    runtime_policy historical_errors{true, true, baseline};
    input = healthy_input(baseline);
    ++input.can.error_count;
    ++input.can.drop_count;
    ++input.can.fault_epoch;
    input.can.bus_state = bsp::can::state::passive;
    output = historical_errors.update(input);
    require(output.safety.can_healthy);
    require(!output.force_zero);
    require(!output.fault_latched);
}

void test_constructor_and_runtime_faults_are_sticky() noexcept
{
    const auto baseline = active_can();

    auto stopped_can = baseline;
    stopped_can.bus_state = bsp::can::state::stopped;
    runtime_policy inactive_can{true, true, stopped_can};
    auto output = inactive_can.update(healthy_input(stopped_can));
    require(output.force_zero);
    require(!output.fault_latched);
    output = inactive_can.update(healthy_input(baseline));
    require(!output.force_zero);
    require(!output.fault_latched);

    runtime_policy invalid_config{false, true, baseline};
    output = invalid_config.update(healthy_input(baseline));
    require(!output.safety.config_valid);
    require(output.force_zero);
    require(output.fault_latched);
    require(has(invalid_config.faults(), runtime_fault::invalid_config));

    runtime_policy registration_failed{true, false, baseline};
    output = registration_failed.update(healthy_input(baseline));
    require(output.force_zero);
    require(output.fault_latched);
    require(has(registration_failed.faults(),
                runtime_fault::registration_failed));

    runtime_policy overrun{true, true, baseline};
    auto input = healthy_input(baseline);
    input.overrun = true;
    output = overrun.update(input);
    require(has(overrun.faults(), runtime_fault::overrun));
    require(output.force_zero);
    require(output.fault_latched);
    input.overrun = false;
    require(overrun.update(input).force_zero);
}

void test_reported_state_override() noexcept
{
    const auto baseline = active_can();
    runtime_policy healthy{true, true, baseline};
    for (const arm_safety_state state : {
             arm_safety_state::disabled,
             arm_safety_state::waiting_remote,
             arm_safety_state::waiting_j1,
             arm_safety_state::armed,
             arm_safety_state::fault_latched,
         })
    {
        require(healthy.reported_state(state) == state);
    }

    healthy.latch(runtime_fault::subscribe_failed);
    require(healthy.reported_state(arm_safety_state::armed) ==
            arm_safety_state::fault_latched);
}

void test_watchdog_phase_and_freshness() noexcept
{
    watchdog_phase phase{};
    for (std::uint32_t cycle = 1U; cycle <= 12U; ++cycle)
    {
        require(phase.advance() == (cycle % 4U == 0U));
    }

    constexpr std::uint32_t freshness_ticks = 120U;
    require(!remote_snapshot_fresh(false, 100U, 100U, freshness_ticks));
    require(remote_snapshot_fresh(true, 100U, 220U, freshness_ticks));
    require(!remote_snapshot_fresh(true, 100U, 221U, freshness_ticks));

    constexpr std::uint32_t wrapped_sample =
        std::numeric_limits<std::uint32_t>::max() - 59U;
    require(remote_snapshot_fresh(
        true, wrapped_sample, 60U, freshness_ticks));
    require(!remote_snapshot_fresh(
        true, wrapped_sample, 61U, freshness_ticks));
}

void test_output_decision_and_release_policy() noexcept
{
    runtime_policy_output policy{};
    policy.manual.online = true;
    policy.manual.enable_switches_up = false;
    policy.manual.mode = control_mode::arm;
    policy.manual.arm_mode_selected = true;
    policy.safety = {true, true, false, true, true, true, true};

    auto safety = controller_safety_for(policy);
    require(safety.remote_online);
    require(safety.j1_online);
    require(safety.can_healthy);
    require(safety.config_valid);
    require(trusted_release_observed(policy));

    policy.fault_latched = true;
    safety = controller_safety_for(policy);
    require(!safety.config_valid);

    policy.force_zero = false;
    require(!should_enable_outputs(policy, arm_safety_state::disabled));
    require(should_enable_outputs(policy, arm_safety_state::armed));
    policy.force_zero = true;
    require(!should_enable_outputs(policy, arm_safety_state::armed));

    policy.manual.online = false;
    require(!trusted_release_observed(policy));
}

void test_wrap_safe_deadline_comparison() noexcept
{
    require(!deadline_reached(99U, 100U));
    require(deadline_reached(100U, 100U));
    require(deadline_reached(101U, 100U));

    constexpr std::uint32_t near_wrap =
        std::numeric_limits<std::uint32_t>::max() - 1U;
    require(!deadline_reached(near_wrap, 2U));
    require(deadline_reached(2U, near_wrap));
}

} // namespace

int main()
{
    test_safety_requires_release_then_rise();
    test_safety_fault_latch_and_reset();
    test_arm_mode_requires_initial_release_and_allows_centered_resume();
    test_arm_mode_resume_is_cleared_by_disarm_or_health_loss();
    test_remote_source_switches_and_axis();
    test_j1_hold_requires_prior_arm_and_healthy_right_switch();
    test_j2_hold_requires_existing_pwm_and_healthy_right_switch();
    test_watchdog_and_can_health();
    test_constructor_and_runtime_faults_are_sticky();
    test_reported_state_override();
    test_watchdog_phase_and_freshness();
    test_output_decision_and_release_policy();
    test_wrap_safe_deadline_comparison();
    return EXIT_SUCCESS;
}
