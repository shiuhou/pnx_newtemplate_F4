#include "vehicle/arm/runtime/arm_runtime.hpp"

namespace vehicle::arm
{
namespace
{

control_mode mode_for(remoter::sw_state left_switch) noexcept
{
    switch (left_switch)
    {
    case remoter::sw_state::up:
        return control_mode::chassis;
    case remoter::sw_state::low:
        return control_mode::arm;
    case remoter::sw_state::mid:
    default:
        return control_mode::neutral;
    }
}

bool can_state_allows_attended_control(bsp::can::state state) noexcept
{
    return state == bsp::can::state::active ||
           state == bsp::can::state::warning ||
           state == bsp::can::state::passive;
}

bool trusted_remote(const remoter::state& remote) noexcept
{
    return !remote.offline &&
           (remote.active_source == remoter::source::dr16 ||
            (remote.active_source == remoter::source::ps2 &&
             remote.ps2_link == remoter::ps2_link_state::connected));
}

} // namespace

runtime_policy::runtime_policy(bool config_valid, bool j1_registered,
                               bsp::can::telemetry) noexcept
    : config_valid_(config_valid)
{
    if (!config_valid_)
    {
        latch(runtime_fault::invalid_config);
    }
    if (!j1_registered)
    {
        latch(runtime_fault::registration_failed);
    }
}

runtime_policy_output runtime_policy::update(
    const runtime_policy_input& input) noexcept
{
    const bool remote_online = trusted_remote(input.remote);
    const control_mode mode = mode_for(input.remote.left_sw);
    const bool arm_mode_selected = mode == control_mode::arm;
    const bool right_switch_up =
        input.remote.right_sw == remoter::sw_state::up;
    const bool enable_switches_up =
        arm_mode_selected && right_switch_up;
    const bool can_healthy =
        can_state_allows_attended_control(input.can.bus_state);
    if (input.overrun)
    {
        latch(runtime_fault::overrun);
    }

    runtime_policy_output output{};
    output.manual = {
        remote_online,
        mode,
        arm_mode_selected,
        right_switch_up,
        enable_switches_up,
        arm_mode_selected ? input.remote.right_y : 0.0F,
        arm_mode_selected ? input.remote.left_y : 0.0F,
        arm_mode_selected ? input.remote.right_x : 0.0F,
        arm_mode_selected ? input.remote.left_x : 0.0F,
    };
    output.safety = {
        remote_online,
        arm_mode_selected,
        right_switch_up,
        input.manual_axes_centered,
        input.watchdog_sampled && input.j1_online,
        can_healthy,
        config_valid_,
    };
    output.safety.mode_independent_unlock =
        remote_online &&
        input.remote.active_source == remoter::source::ps2;
    output.safety.right_switch_up = right_switch_up;
    output.fault_latched = fault_latched();
    output.force_zero = output.fault_latched || !arm_mode_selected ||
                        !output.safety.remote_online ||
                        !output.safety.j1_online ||
                        !output.safety.can_healthy ||
                        !output.safety.config_valid;
    return output;
}

bool j1_hold_gate::update(const j1_hold_input& input) noexcept
{
    const bool hold_healthy =
        input.remote_online && input.right_switch_up && input.j1_online &&
        input.can_healthy && input.config_valid && !input.fault_latched;
    if (!hold_healthy)
    {
        latched_ = false;
        return false;
    }

    if (input.arm_control_enabled)
    {
        latched_ = true;
    }
    return latched_;
}

void j1_hold_gate::reset() noexcept
{
    latched_ = false;
}

void runtime_policy::latch(runtime_fault fault) noexcept
{
    fault_mask_ |= static_cast<std::uint32_t>(fault);
}

bool runtime_policy::fault_latched() const noexcept
{
    return fault_mask_ != 0U;
}

runtime_fault runtime_policy::faults() const noexcept
{
    return static_cast<runtime_fault>(fault_mask_);
}

arm_safety_state runtime_policy::reported_state(
    arm_safety_state controller_state) const noexcept
{
    return fault_latched() ? arm_safety_state::fault_latched
                           : controller_state;
}

bool watchdog_phase::advance() noexcept
{
    ++phase_;
    if (phase_ == 4U)
    {
        phase_ = 0U;
        return true;
    }
    return false;
}

bool remote_snapshot_fresh(bool seen, std::uint32_t sample_tick,
                           std::uint32_t now,
                           std::uint32_t freshness_ticks) noexcept
{
    return seen && (now - sample_tick) <= freshness_ticks;
}

bool should_enable_outputs(const runtime_policy_output& policy,
                           arm_safety_state controller_state) noexcept
{
    return !policy.force_zero && controller_state == arm_safety_state::armed;
}

bool should_hold_j2_output(bool pwm_active,
                           const runtime_policy_output& policy) noexcept
{
    return pwm_active && policy.manual.online &&
           policy.manual.right_switch_up && policy.safety.j1_online &&
           policy.safety.can_healthy && policy.safety.config_valid &&
           !policy.fault_latched;
}

arm_safety_input controller_safety_for(
    const runtime_policy_output& policy) noexcept
{
    arm_safety_input result = policy.safety;
    result.config_valid = result.config_valid && !policy.fault_latched;
    return result;
}

bool trusted_release_observed(
    const runtime_policy_output& policy) noexcept
{
    return policy.manual.online && policy.manual.arm_mode_selected &&
           !policy.manual.enable_switches_up;
}

bool deadline_reached(std::uint32_t now,
                      std::uint32_t deadline) noexcept
{
    return static_cast<std::int32_t>(now - deadline) >= 0;
}

} // namespace vehicle::arm
