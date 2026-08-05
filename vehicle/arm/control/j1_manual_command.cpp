#include "vehicle/arm/control/j1_manual_command.hpp"

#include <algorithm>
#include <cmath>

namespace vehicle::arm
{

j1_manual_command::j1_manual_command(
    position_pid_config outer_loop,
    float manual_position_rate_rad_per_s,
    float position_min_rad,
    float position_max_rad,
    float deadband) noexcept
    : outer_loop_(outer_loop),
      manual_position_rate_rad_per_s_(manual_position_rate_rad_per_s),
      position_min_rad_(position_min_rad),
      position_max_rad_(position_max_rad),
      deadband_(deadband),
      config_valid_(valid(outer_loop) &&
                    std::isfinite(manual_position_rate_rad_per_s_) &&
                    manual_position_rate_rad_per_s_ > 0.0F &&
                    std::isfinite(position_min_rad_) &&
                    std::isfinite(position_max_rad_) &&
                    position_min_rad_ < position_max_rad_ &&
                    std::isfinite(deadband_) && deadband_ >= 0.0F &&
                    deadband_ < 1.0F)
{
}

j1_manual_command_output j1_manual_command::update(
    const j1_manual_command_input& input) noexcept
{
    if (!input.enabled)
    {
        reset();
        return output_;
    }

    if (!config_valid_ || !std::isfinite(input.manual_axis) ||
        !std::isfinite(input.measured_position_rad) ||
        !std::isfinite(input.measured_velocity_rad_s) ||
        !std::isfinite(input.dt_s) || input.dt_s <= 0.0F)
    {
        reset();
        return output_;
    }

    if (!target_position_seeded_)
    {
        output_.target_position_rad = std::clamp(
            input.measured_position_rad, position_min_rad_,
            position_max_rad_);
        target_position_seeded_ = true;
    }

    float axis = std::clamp(input.manual_axis, -1.0F, 1.0F);
    if (std::fabs(axis) <= deadband_)
    {
        axis = 0.0F;
    }

    const float candidate = output_.target_position_rad +
                            axis * manual_position_rate_rad_per_s_ *
                                input.dt_s;
    if (!std::isfinite(candidate))
    {
        reset();
        return output_;
    }
    output_.target_position_rad = std::clamp(
        candidate, position_min_rad_, position_max_rad_);
    output_.target_velocity_rad_s = outer_loop_.update(
        output_.target_position_rad, input.measured_position_rad,
        input.measured_velocity_rad_s, input.dt_s);
    if (!std::isfinite(output_.target_velocity_rad_s))
    {
        reset();
    }
    return output_;
}

void j1_manual_command::reset() noexcept
{
    outer_loop_.reset();
    output_ = {};
    target_position_seeded_ = false;
}

} // namespace vehicle::arm
