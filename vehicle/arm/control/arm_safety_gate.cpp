#include "vehicle/arm/control/arm_safety_gate.hpp"

namespace vehicle::arm
{

arm_safety_state arm_safety_gate::update(const arm_safety_input& input) noexcept
{
    if (!input.remote_online || !input.j1_online || !input.can_healthy ||
        !input.config_valid)
    {
        arm_position_released_ = false;
    }

    const bool switch_sample_valid =
        input.remote_online && input.arm_mode_selected;
    const bool arm_rising = switch_sample_valid &&
                            input.enable_switches_up &&
                            !previous_enable_switches_up_;
    const bool release_observed =
        input.manual_axes_centered &&
        ((switch_sample_valid && !input.enable_switches_up) ||
         (input.mode_independent_unlock && !input.right_switch_up));

    if (release_observed)
    {
        arm_position_released_ = true;
        if (state_ == arm_safety_state::fault_latched)
        {
            fault_release_observed_ = true;
        }
    }

    if (state_ != arm_safety_state::fault_latched &&
        input.remote_online && !input.arm_mode_selected)
    {
        state_ = arm_safety_state::disabled;
        if (!input.mode_independent_unlock)
        {
            arm_position_released_ =
                arm_position_released_ && input.enable_switches_up;
        }
        previous_enable_switches_up_ = false;
        return state_;
    }

    if (state_ == arm_safety_state::fault_latched)
    {
        if (switch_sample_valid)
        {
            previous_enable_switches_up_ = input.enable_switches_up;
        }
        return state_;
    }

    if (state_ == arm_safety_state::armed)
    {
        if (!input.config_valid || !input.remote_online || !input.j1_online ||
            !input.can_healthy)
        {
            state_ = arm_safety_state::fault_latched;
            fault_release_observed_ =
                switch_sample_valid && !input.enable_switches_up;
        }
        else if (!input.enable_switches_up)
        {
            state_ = arm_safety_state::disabled;
        }

        if (switch_sample_valid)
        {
            previous_enable_switches_up_ = input.enable_switches_up;
        }
        return state_;
    }

    if (!input.config_valid)
    {
        state_ = arm_safety_state::disabled;
    }
    else if (!input.remote_online || !input.arm_mode_selected)
    {
        state_ = arm_safety_state::waiting_remote;
    }
    else if (!input.j1_online || !input.can_healthy)
    {
        state_ = arm_safety_state::waiting_j1;
    }
    else if (arm_position_released_ && arm_rising &&
             input.manual_axes_centered)
    {
        state_ = arm_safety_state::armed;
    }
    else
    {
        state_ = arm_safety_state::disabled;
    }

    if (switch_sample_valid)
    {
        previous_enable_switches_up_ = input.enable_switches_up;
    }
    return state_;
}

bool arm_safety_gate::output_enabled() const noexcept
{
    return state_ == arm_safety_state::armed;
}

void arm_safety_gate::reset() noexcept
{
    if (state_ == arm_safety_state::fault_latched && fault_release_observed_)
    {
        state_ = arm_safety_state::disabled;
        fault_release_observed_ = false;
    }
}

} // namespace vehicle::arm
