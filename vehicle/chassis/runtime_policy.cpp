#include "vehicle/chassis/runtime.hpp"

namespace vehicle::chassis
{

runtime_policy::runtime_policy(bool config_valid, bool all_registered,
                               bsp::can::telemetry can_baseline) noexcept
    : can_baseline_(can_baseline), config_valid_(config_valid)
{
    if (!config_valid_)
    {
        latch(runtime_fault::invalid_config);
    }
    if (!all_registered)
    {
        latch(runtime_fault::registration_failed);
    }
    if (can_baseline_.bus_state != bsp::can::state::active)
    {
        latch(runtime_fault::can_changed);
    }
}

runtime_policy_output runtime_policy::update(
    const runtime_policy_input& input) noexcept
{
    const bool remote_online =
        !input.remote.offline &&
        input.remote.active_source == remoter::source::dr16;
    const bool arm_switches_up =
        input.remote.left_sw == remoter::sw_state::up &&
        input.remote.right_sw == remoter::sw_state::up;
    const bool retained_can_unchanged =
        input.can.error_count == can_baseline_.error_count &&
        input.can.drop_count == can_baseline_.drop_count &&
        input.can.fault_epoch == can_baseline_.fault_epoch;
    const bool can_healthy =
        input.can.bus_state == bsp::can::state::active &&
        retained_can_unchanged;

    if (!retained_can_unchanged)
    {
        latch(runtime_fault::can_changed);
    }
    if (input.overrun)
    {
        latch(runtime_fault::overrun);
    }

    runtime_policy_output output{};
    output.manual = {
        remote_online,
        arm_switches_up,
        input.remote.left_x,
        input.remote.left_y,
        input.remote.right_x,
    };
    output.safety = {
        remote_online,
        arm_switches_up,
        input.watchdog_sampled && input.handler_all_online,
        can_healthy,
        config_valid_,
    };
    output.fault_latched = fault_latched();
    output.force_zero = output.fault_latched || !output.safety.can_healthy ||
                        !output.safety.all_motors_online ||
                        !output.safety.remote_online ||
                        !output.safety.config_valid;
    return output;
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

safety_state runtime_policy::reported_state(
    safety_state controller_state) const noexcept
{
    return fault_latched() ? safety_state::fault_latched : controller_state;
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

bool should_set_current(const runtime_policy_output& policy,
                        safety_state controller_state) noexcept
{
    return !policy.force_zero && controller_state == safety_state::armed;
}

safety_input controller_safety_for(
    const runtime_policy_output& policy) noexcept
{
    safety_input result = policy.safety;
    result.config_valid = result.config_valid && !policy.fault_latched;
    return result;
}

bool trusted_release_observed(
    const runtime_policy_output& policy) noexcept
{
    return policy.manual.online && !policy.manual.arm_switches_up;
}

bool deadline_reached(std::uint32_t now, std::uint32_t deadline) noexcept
{
    return static_cast<std::int32_t>(now - deadline) >= 0;
}

} // namespace vehicle::chassis
