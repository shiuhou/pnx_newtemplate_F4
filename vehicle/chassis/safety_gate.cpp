#include "vehicle/chassis/safety_gate.hpp"

namespace vehicle::chassis
{

safety_state safety_gate::update(const safety_input& input) noexcept
{
    const bool switch_sample_valid = input.remote_online;
    const bool arm_rising = switch_sample_valid &&
                            input.arm_switches_up &&
                            !previous_arm_switches_up_;

    if (switch_sample_valid && !input.arm_switches_up)
    {
        arm_position_released_ = true;
        if (state_ == safety_state::fault_latched)
        {
            fault_release_observed_ = true;
        }
    }

    if (state_ == safety_state::fault_latched)
    {
        if (switch_sample_valid)
        {
            previous_arm_switches_up_ = input.arm_switches_up;
        }
        return state_;
    }

    if (state_ == safety_state::armed)
    {
        if (!input.config_valid || !input.remote_online ||
            !input.all_motors_online || !input.can_healthy)
        {
            state_ = safety_state::fault_latched;
            fault_release_observed_ =
                switch_sample_valid && !input.arm_switches_up;
        }
        else if (!input.arm_switches_up)
        {
            state_ = safety_state::disabled;
        }

        if (switch_sample_valid)
        {
            previous_arm_switches_up_ = input.arm_switches_up;
        }
        return state_;
    }

    if (!input.config_valid)
    {
        state_ = safety_state::disabled;
    }
    else if (!input.remote_online)
    {
        state_ = safety_state::waiting_remote;
    }
    else if (!input.all_motors_online || !input.can_healthy)
    {
        state_ = safety_state::waiting_motors;
    }
    else if (arm_position_released_ && arm_rising)
    {
        state_ = safety_state::armed;
    }
    else
    {
        state_ = safety_state::disabled;
    }

    if (switch_sample_valid)
    {
        previous_arm_switches_up_ = input.arm_switches_up;
    }
    return state_;
}

bool safety_gate::output_enabled() const noexcept
{
    return state_ == safety_state::armed;
}

void safety_gate::reset() noexcept
{
    if (state_ == safety_state::fault_latched && fault_release_observed_)
    {
        state_ = safety_state::disabled;
        fault_release_observed_ = false;
    }
}

} // namespace vehicle::chassis
