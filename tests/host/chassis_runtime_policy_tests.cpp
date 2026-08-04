#include "vehicle/chassis/runtime/runtime.hpp"

// 可執行規格：DR16／CAN／watchdog／overrun 任一健康條件失效時 runtime 必須 force zero。

#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <type_traits>

namespace
{

using namespace vehicle::chassis;

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
    value.last_id = 0x204U;
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
    value.left_sw = remoter::sw_state::up;
    value.right_sw = remoter::sw_state::up;
    value.left_x = -0.25F;
    value.left_y = 0.50F;
    value.right_x = 0.75F;
    return value;
}

runtime_policy_input healthy_input(
    const bsp::can::telemetry& can_baseline) noexcept
{
    runtime_policy_input value{};
    value.remote = online_dr16();
    value.can = can_baseline;
    value.watchdog_sampled = true;
    value.handler_all_online = true;
    return value;
}

void require_healthy_output(const runtime_policy_output& output) noexcept
{
    require(output.manual.online);
    require(output.manual.arm_switches_up);
    require(output.manual.left_x == 0.50F);
    require(output.manual.left_y == -0.25F);
    require(output.manual.right_x == 0.75F);
    require(output.safety.remote_online);
    require(output.safety.arm_switches_up);
    require(output.safety.all_motors_online);
    require(output.safety.can_healthy);
    require(output.safety.config_valid);
    require(!output.force_zero);
    require(!output.fault_latched);
}

void test_remote_source_switches_and_axes() noexcept
{
    const auto baseline = active_can();

    runtime_policy offline_policy{true, true, baseline};
    auto input = healthy_input(baseline);
    input.remote.offline = true;
    auto output = offline_policy.update(input);
    require(!output.manual.online);
    require(!output.safety.remote_online);
    require(output.force_zero);
    require(!output.fault_latched);

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
            const bool right_up = right == remoter::sw_state::up;
            require(output.manual.arm_switches_up == right_up);
            require(output.safety.arm_switches_up == right_up);
        }
    }

    runtime_policy axes_policy{true, true, baseline};
    require_healthy_output(axes_policy.update(healthy_input(baseline)));
}

void test_each_non_active_can_state_forces_zero_without_latching() noexcept
{
    const auto baseline = active_can();
    for (const bsp::can::state state : {
             bsp::can::state::stopped,
             bsp::can::state::warning,
             bsp::can::state::passive,
             bsp::can::state::bus_off,
             bsp::can::state::recovering,
             bsp::can::state::fault,
         })
    {
        runtime_policy policy{true, true, baseline};
        auto input = healthy_input(baseline);
        input.can.bus_state = state;
        const auto output = policy.update(input);
        require(!output.safety.can_healthy);
        require(output.force_zero);
        require(!output.fault_latched);
        require(policy.faults() == runtime_fault::none);
    }
}

void test_can_retained_deltas_latch_individually() noexcept
{
    const auto baseline = active_can();

    for (std::size_t changed = 0U; changed < 3U; ++changed)
    {
        runtime_policy policy{true, true, baseline};
        auto input = healthy_input(baseline);
        if (changed == 0U)
        {
            ++input.can.error_count;
        }
        else if (changed == 1U)
        {
            ++input.can.drop_count;
        }
        else
        {
            ++input.can.fault_epoch;
        }

        const auto faulted = policy.update(input);
        require(!faulted.safety.can_healthy);
        require(faulted.force_zero);
        require(faulted.fault_latched);
        require(has(policy.faults(), runtime_fault::can_changed));

        const auto restored = policy.update(healthy_input(baseline));
        require(restored.safety.can_healthy);
        require(restored.force_zero);
        require(restored.fault_latched);
        require(policy.reported_state(safety_state::armed) ==
                safety_state::fault_latched);
    }
}

void test_can_non_retained_telemetry_is_ignored() noexcept
{
    const auto baseline = active_can();
    runtime_policy policy{true, true, baseline};
    auto input = healthy_input(baseline);
    input.can.rx_count += 100U;
    input.can.tx_count += 100U;
    input.can.last_id = 0x777U;
    input.can.last_tick += 100U;
    input.can.bus_off_count += 100U;
    require_healthy_output(policy.update(input));
    require(policy.faults() == runtime_fault::none);
}

void test_watchdog_provenance() noexcept
{
    const auto baseline = active_can();

    runtime_policy unsampled{true, true, baseline};
    auto input = healthy_input(baseline);
    input.watchdog_sampled = false;
    input.handler_all_online = true;
    auto output = unsampled.update(input);
    require(!output.safety.all_motors_online);
    require(output.force_zero);
    require(!output.fault_latched);

    runtime_policy failed{true, true, baseline};
    input = healthy_input(baseline);
    input.handler_all_online = false;
    output = failed.update(input);
    require(!output.safety.all_motors_online);
    require(output.force_zero);
    require(!output.fault_latched);

    runtime_policy sampled{true, true, baseline};
    require_healthy_output(sampled.update(healthy_input(baseline)));
}

void test_constructor_and_runtime_faults_are_sticky() noexcept
{
    const auto baseline = active_can();

    auto inactive_baseline = baseline;
    inactive_baseline.bus_state = bsp::can::state::stopped;
    runtime_policy inactive_can{true, true, inactive_baseline};
    auto output = inactive_can.update(healthy_input(inactive_baseline));
    require(!output.safety.can_healthy);
    require(output.force_zero);
    require(output.fault_latched);
    require(has(inactive_can.faults(), runtime_fault::can_changed));

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

    for (const runtime_fault fault : {
             runtime_fault::remoter_init_failed,
             runtime_fault::subscribe_failed,
             runtime_fault::thread_create_failed,
             runtime_fault::can_changed,
             runtime_fault::overrun,
         })
    {
        runtime_policy policy{true, true, baseline};
        policy.latch(fault);
        require(has(policy.faults(), fault));
        require(policy.fault_latched());
        require(policy.update(healthy_input(baseline)).force_zero);
    }

    runtime_policy overrun{true, true, baseline};
    auto input = healthy_input(baseline);
    input.overrun = true;
    output = overrun.update(input);
    require(has(overrun.faults(), runtime_fault::overrun));
    require(output.force_zero);
    require(output.fault_latched);
    input.overrun = false;
    require(overrun.update(input).force_zero);

    runtime_policy multiple{false, false, baseline};
    require(has(multiple.faults(), runtime_fault::invalid_config));
    require(has(multiple.faults(), runtime_fault::registration_failed));
    multiple.latch(runtime_fault::none);
    require(multiple.fault_latched());
}

void test_reported_state_override() noexcept
{
    const auto baseline = active_can();
    runtime_policy healthy{true, true, baseline};
    for (const safety_state state : {
             safety_state::disabled,
             safety_state::waiting_remote,
             safety_state::waiting_motors,
             safety_state::armed,
             safety_state::fault_latched,
         })
    {
        require(healthy.reported_state(state) == state);
    }

    healthy.latch(runtime_fault::subscribe_failed);
    for (const safety_state state : {
             safety_state::disabled,
             safety_state::waiting_remote,
             safety_state::waiting_motors,
             safety_state::armed,
             safety_state::fault_latched,
         })
    {
        require(healthy.reported_state(state) == safety_state::fault_latched);
    }
}

void test_watchdog_phase() noexcept
{
    watchdog_phase phase{};
    for (std::uint32_t cycle = 1U; cycle <= 12U; ++cycle)
    {
        require(phase.advance() == (cycle % 4U == 0U));
    }

    watchdog_phase across_loop_count_rollover{};
    std::uint32_t loop_count =
        std::numeric_limits<std::uint32_t>::max() - 2U;
    for (std::uint32_t step = 1U; step <= 8U; ++step)
    {
        ++loop_count;
        require(across_loop_count_rollover.advance() == (step % 4U == 0U));
    }
    require(loop_count == 5U);
}

void test_remote_snapshot_freshness() noexcept
{
    constexpr std::uint32_t freshness_ticks = 120U;
    require(!remote_snapshot_fresh(false, 100U, 100U, freshness_ticks));
    require(remote_snapshot_fresh(true, 100U, 100U, freshness_ticks));
    require(remote_snapshot_fresh(true, 100U, 220U, freshness_ticks));
    require(!remote_snapshot_fresh(true, 100U, 221U, freshness_ticks));

    constexpr std::uint32_t wrapped_sample =
        std::numeric_limits<std::uint32_t>::max() - 59U;
    require(remote_snapshot_fresh(
        true, wrapped_sample, 60U, freshness_ticks));
    require(!remote_snapshot_fresh(
        true, wrapped_sample, 61U, freshness_ticks));
}

void test_output_decision() noexcept
{
    runtime_policy_output allow{};
    allow.force_zero = false;
    for (const safety_state state : {
             safety_state::disabled,
             safety_state::waiting_remote,
             safety_state::waiting_motors,
             safety_state::fault_latched,
         })
    {
        require(!should_set_current(allow, state));
    }
    require(should_set_current(allow, safety_state::armed));

    allow.force_zero = true;
    require(!should_set_current(allow, safety_state::armed));
}

void test_controller_inhibit_and_trusted_release_policy() noexcept
{
    runtime_policy_output policy{};
    policy.manual.online = true;
    policy.manual.arm_switches_up = false;
    policy.safety = {true, false, true, true, true};

    auto safety = controller_safety_for(policy);
    require(safety.remote_online);
    require(safety.all_motors_online);
    require(safety.can_healthy);
    require(safety.config_valid);
    require(trusted_release_observed(policy));

    policy.fault_latched = true;
    safety = controller_safety_for(policy);
    require(!safety.config_valid);

    policy.manual.online = false;
    require(!trusted_release_observed(policy));
    policy.manual.online = true;
    policy.manual.arm_switches_up = true;
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
    test_remote_source_switches_and_axes();
    test_each_non_active_can_state_forces_zero_without_latching();
    test_can_retained_deltas_latch_individually();
    test_can_non_retained_telemetry_is_ignored();
    test_watchdog_provenance();
    test_constructor_and_runtime_faults_are_sticky();
    test_reported_state_override();
    test_watchdog_phase();
    test_remote_snapshot_freshness();
    test_output_decision();
    test_controller_inhibit_and_trusted_release_policy();
    test_wrap_safe_deadline_comparison();
    return EXIT_SUCCESS;
}
